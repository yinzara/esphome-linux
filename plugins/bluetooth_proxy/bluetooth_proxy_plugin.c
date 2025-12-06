/**
 * @file bluetooth_proxy_plugin.c
 * @brief Bluetooth Proxy Plugin for ESPHome
 *
 * Implements Bluetooth LE scanning and forwarding to Home Assistant
 * via ESPHome Native API.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include "../../src/include/esphome_plugin.h"
#include "../../src/include/esphome_api.h"
#include "../../src/include/esphome_proto.h"
#include "ble_scanner.h"
#include "switch_proto.h"

/* Entity keys for this plugin */
static const uint32_t ble_scanning_switch_key = 100;  /* Unique key for BLE scanning switch */

/* BLE Advertisement batching configuration */
#define BLE_MAX_ADV_BATCH 16
#define BLE_BATCH_FLUSH_INTERVAL_MS 100

/**
 * Plugin state (needs context reference for flush thread)
 */
typedef struct {
    ble_scanner_t *scanner;
    bool subscribed;

    /* BLE advertisement batching */
    esphome_ble_advertisements_response_t ble_batch;
    pthread_mutex_t batch_mutex;
    struct timespec last_flush;
    pthread_t flush_thread;
    bool flush_thread_running;

    /* Context reference (for flush thread) */
    esphome_plugin_context_t *ctx;

    /* BLE Scanning switch state - enabled by default */
    bool ble_scanning_enabled;
} bluetooth_proxy_state_t;

/**
 * Convert MAC address to uint64 (big-endian)
 */
static uint64_t mac_to_uint64(const uint8_t *mac) {
    uint64_t result = 0;
    for (int i = 0; i < 6; i++) {
        result |= ((uint64_t)mac[i]) << ((5 - i) * 8);
    }
    return result;
}

/**
 * Flush BLE batch to all connected clients
 */
static void flush_ble_batch(bluetooth_proxy_state_t *state, esphome_plugin_context_t *ctx) {
    pthread_mutex_lock(&state->batch_mutex);

    if (state->ble_batch.count == 0) {
        pthread_mutex_unlock(&state->batch_mutex);
        return;
    }

    /* Encode advertisements */
    uint8_t encode_buf[4096];
    size_t len = esphome_encode_ble_advertisements(encode_buf, sizeof(encode_buf),
                                                     &state->ble_batch);

    if (len > 0) {
        /* Broadcast to all clients */
        esphome_plugin_send_message(ctx, ESPHOME_MSG_BLUETOOTH_LE_RAW_ADVERTISEMENTS_RESPONSE,
                                    encode_buf, len);

        printf("[bluetooth_proxy] Sent BLE batch: %zu advertisements\n", state->ble_batch.count);
    }

    /* Reset batch */
    state->ble_batch.count = 0;
    clock_gettime(CLOCK_MONOTONIC, &state->last_flush);

    pthread_mutex_unlock(&state->batch_mutex);
}

/**
 * Batch flush thread - periodically flushes BLE advertisements
 */
static void *flush_thread_func(void *arg) {
    bluetooth_proxy_state_t *state = (bluetooth_proxy_state_t *)arg;

    /* Use shorter sleep intervals to check stop flag more frequently */
    const int sleep_interval_ms = 10;
    int sleep_count = 0;

    while (state->flush_thread_running) {
        usleep(sleep_interval_ms * 1000);
        sleep_count++;

        /* Check stop flag frequently */
        if (!state->flush_thread_running) {
            break;
        }

        /* Only check flush interval every BLE_BATCH_FLUSH_INTERVAL_MS */
        if (sleep_count * sleep_interval_ms < BLE_BATCH_FLUSH_INTERVAL_MS) {
            continue;
        }
        sleep_count = 0;

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        uint64_t elapsed_ms = (now.tv_sec - state->last_flush.tv_sec) * 1000 +
                             (now.tv_nsec - state->last_flush.tv_nsec) / 1000000;

        if (elapsed_ms >= BLE_BATCH_FLUSH_INTERVAL_MS && state->ble_batch.count > 0) {
            flush_ble_batch(state, state->ctx);
        }
    }

    return NULL;
}

/**
 * BLE advertisement callback - queues for batching
 */
static void on_ble_advertisement(const ble_advertisement_t *advert, void *user_data) {
    esphome_plugin_context_t *ctx = (esphome_plugin_context_t *)user_data;
    bluetooth_proxy_state_t *state = (bluetooth_proxy_state_t *)ctx->plugin_data;

    pthread_mutex_lock(&state->batch_mutex);

    /* Flush if batch is full */
    if (state->ble_batch.count >= BLE_MAX_ADV_BATCH) {
        pthread_mutex_unlock(&state->batch_mutex);
        flush_ble_batch(state, ctx);
        pthread_mutex_lock(&state->batch_mutex);
    }

    /* Add to batch */
    esphome_ble_advertisement_t *pb_adv = &state->ble_batch.advertisements[state->ble_batch.count];

    pb_adv->address = mac_to_uint64(advert->address);
    pb_adv->rssi = advert->rssi;
    pb_adv->address_type = advert->address_type;

    size_t copy_len = advert->data_len;
    if (copy_len > sizeof(pb_adv->data)) {
        copy_len = sizeof(pb_adv->data);
    }

    memcpy(pb_adv->data, advert->data, copy_len);
    pb_adv->data_len = copy_len;

    state->ble_batch.count++;

    pthread_mutex_unlock(&state->batch_mutex);

    /* Flush immediately if batch is full */
    if (state->ble_batch.count >= BLE_MAX_ADV_BATCH) {
        flush_ble_batch(state, ctx);
    }
}

/**
 * Report BLE scanning switch state to Home Assistant
 */
static void report_ble_scanning_switch_state(esphome_plugin_context_t *ctx, bool state) {
    uint8_t encode_buf[128];
    switch_state_response_t state_msg = {
        .key = ble_scanning_switch_key,
        .state = state
    };
    size_t len = switch_encode_state_response(encode_buf, sizeof(encode_buf), &state_msg);

    if (len > 0) {
        esphome_plugin_send_message(ctx, ESPHOME_MSG_SWITCH_STATE_RESPONSE,
                                    encode_buf, len);
        printf("[bluetooth_proxy] Reported BLE scanning switch state: %s\n",
               state ? "ON" : "OFF");
    } else {
        fprintf(stderr, "[bluetooth_proxy] Failed to encode switch state response\n");
    }
}

/**
 * Start BLE scanning (internal helper)
 */
static int start_ble_scanning(bluetooth_proxy_state_t *state) {
    if (!state || !state->scanner) {
        return -1;
    }

    if (state->subscribed) {
        return 0;  /* Already scanning */
    }

    if (ble_scanner_start(state->scanner) < 0) {
        fprintf(stderr, "[bluetooth_proxy] Failed to start BLE scanning\n");
        return -1;
    }

    state->subscribed = true;
    printf("[bluetooth_proxy] BLE scanning started\n");
    return 0;
}

/**
 * Stop BLE scanning (internal helper)
 */
static int stop_ble_scanning(bluetooth_proxy_state_t *state) {
    if (!state || !state->scanner) {
        return 0;
    }

    if (!state->subscribed) {
        return 0;  /* Already stopped */
    }

    if (ble_scanner_stop(state->scanner) < 0) {
        fprintf(stderr, "[bluetooth_proxy] Failed to stop BLE scanning\n");
        return -1;
    }

    state->subscribed = false;
    printf("[bluetooth_proxy] BLE scanning stopped\n");
    return 0;
}

/**
 * Configure device info with Bluetooth proxy capabilities
 */
static int bluetooth_proxy_configure_device_info(esphome_plugin_context_t *ctx,
                                                   esphome_device_info_response_t *device_info) {
    (void)ctx;

    /* Advertise Bluetooth proxy support - passive scanning + raw advertisements only */
    device_info->bluetooth_proxy_feature_flags = BLE_FEATURE_PASSIVE_SCAN | BLE_FEATURE_RAW_ADVERTISEMENTS;

    /* Use the same MAC address for Bluetooth */
    strncpy(device_info->bluetooth_mac_address, ctx->config->mac_address,
            sizeof(device_info->bluetooth_mac_address) - 1);

    printf("[bluetooth_proxy] Configured device info: BLE proxy flags = 0x%08x\n",
           device_info->bluetooth_proxy_feature_flags);

    return 0;
}

/**
 * Initialize the Bluetooth Proxy plugin
 */
static int bluetooth_proxy_init(esphome_plugin_context_t *ctx) {
    printf("[bluetooth_proxy] Initializing plugin\n");

    /* Allocate plugin state */
    bluetooth_proxy_state_t *state = calloc(1, sizeof(bluetooth_proxy_state_t));
    if (!state) {
        fprintf(stderr, "[bluetooth_proxy] Failed to allocate state\n");
        return -1;
    }

    state->subscribed = false;
    state->ctx = ctx;
    state->ble_scanning_enabled = true;  /* Enabled by default */

    /* Initialize batching system */
    memset(&state->ble_batch, 0, sizeof(state->ble_batch));
    pthread_mutex_init(&state->batch_mutex, NULL);
    clock_gettime(CLOCK_MONOTONIC, &state->last_flush);

    /* Start flush thread */
    state->flush_thread_running = true;
    if (pthread_create(&state->flush_thread, NULL, flush_thread_func, state) != 0) {
        fprintf(stderr, "[bluetooth_proxy] Failed to create flush thread\n");
        pthread_mutex_destroy(&state->batch_mutex);
        free(state);
        return -1;
    }

    /* Initialize BLE scanner */
    state->scanner = ble_scanner_init(on_ble_advertisement, ctx);
    if (!state->scanner) {
        fprintf(stderr, "[bluetooth_proxy] Warning: Failed to initialize BLE scanner\n");
        fprintf(stderr, "[bluetooth_proxy] Plugin will run without BLE scanning\n");
        /* Don't fail - plugin can still handle subscription messages */
    }

    /* Store in context */
    ctx->plugin_data = state;

    printf("[bluetooth_proxy] Plugin initialized successfully\n");
    printf("[bluetooth_proxy] Device: %s\n", ctx->config->device_name);

    return 0;
}

/**
 * Cleanup the Bluetooth Proxy plugin
 */
static void bluetooth_proxy_cleanup(esphome_plugin_context_t *ctx) {
    printf("[bluetooth_proxy] Cleaning up plugin\n");

    if (ctx->plugin_data) {
        bluetooth_proxy_state_t *state = (bluetooth_proxy_state_t *)ctx->plugin_data;

        /* Stop flush thread */
        if (state->flush_thread_running) {
            state->flush_thread_running = false;
            pthread_join(state->flush_thread, NULL);
        }

        if (state->scanner) {
            ble_scanner_stop(state->scanner);
            ble_scanner_free(state->scanner);
        }

        /* Cleanup batching */
        pthread_mutex_destroy(&state->batch_mutex);

        free(state);
        ctx->plugin_data = NULL;
    }
}

/**
 * Handle subscribe to BLE advertisements request
 */
static int handle_subscribe_ble_advertisements(esphome_plugin_context_t *ctx) {
    bluetooth_proxy_state_t *state = (bluetooth_proxy_state_t *)ctx->plugin_data;

    printf("[bluetooth_proxy] Received SUBSCRIBE_BLUETOOTH_LE_ADVERTISEMENTS_REQUEST\n");

    if (!state) {
        fprintf(stderr, "[bluetooth_proxy] Cannot subscribe: plugin state not initialized\n");
        return -1;
    }

    if (!state->scanner) {
        fprintf(stderr, "[bluetooth_proxy] Cannot subscribe: BLE scanner not initialized\n");
        return -1;
    }

    /* Only start scanning if the switch is enabled */
    if (!state->ble_scanning_enabled) {
        printf("[bluetooth_proxy] BLE scanning is disabled by switch, not starting\n");
        return 0;
    }

    return start_ble_scanning(state);
}

/**
 * Handle unsubscribe from BLE advertisements request
 */
static int handle_unsubscribe_ble_advertisements(esphome_plugin_context_t *ctx) {
    bluetooth_proxy_state_t *state = (bluetooth_proxy_state_t *)ctx->plugin_data;

    printf("[bluetooth_proxy] Received UNSUBSCRIBE_BLUETOOTH_LE_ADVERTISEMENTS_REQUEST\n");

    if (!state || !state->scanner) {
        return 0; /* Nothing to do */
    }

    return stop_ble_scanning(state);
}

/**
 * Handle BLE scanning switch command
 */
static int handle_ble_scanning_switch_command(esphome_plugin_context_t *ctx,
                                               const uint8_t *data, size_t len) {
    bluetooth_proxy_state_t *state = (bluetooth_proxy_state_t *)ctx->plugin_data;

    if (!state) {
        fprintf(stderr, "[bluetooth_proxy] Cannot handle switch command: state not initialized\n");
        return -1;
    }

    /* Decode the switch command */
    switch_command_request_t cmd;
    if (!switch_decode_command_request(data, len, &cmd)) {
        fprintf(stderr, "[bluetooth_proxy] Failed to decode switch command\n");
        return -1;
    }

    /* Check if this command is for our switch */
    if (cmd.key != ble_scanning_switch_key) {
        return -1;  /* Not our switch */
    }

    printf("[bluetooth_proxy] BLE scanning switch command: %s\n", cmd.state ? "ON" : "OFF");

    /* Update the enabled state */
    state->ble_scanning_enabled = cmd.state;

    /* Start or stop scanning based on the new state */
    if (cmd.state) {
        /* Enable scanning */
        if (state->scanner) {
            start_ble_scanning(state);
        }
    } else {
        /* Disable scanning */
        stop_ble_scanning(state);
    }

    /* Report the new state back to Home Assistant */
    report_ble_scanning_switch_state(ctx, state->ble_scanning_enabled);

    return 0;
}

/**
 * Message handler - called for each incoming message
 */
static int bluetooth_proxy_handle_message(esphome_plugin_context_t *ctx,
                                           int client_id,
                                           uint32_t msg_type,
                                           const uint8_t *data,
                                           size_t len) {
    (void)client_id;  /* Currently unused, but available for future use */

    switch (msg_type) {
    case ESPHOME_MSG_SUBSCRIBE_BLUETOOTH_LE_ADVERTISEMENTS_REQUEST:
        return handle_subscribe_ble_advertisements(ctx);

    case ESPHOME_MSG_UNSUBSCRIBE_BLUETOOTH_LE_ADVERTISEMENTS_REQUEST:
        return handle_unsubscribe_ble_advertisements(ctx);

    case ESPHOME_MSG_SWITCH_COMMAND_REQUEST:
        return handle_ble_scanning_switch_command(ctx, data, len);

    /* Other Bluetooth messages could be handled here in the future:
     * - BLUETOOTH_DEVICE_REQUEST (connect to device)
     * - BLUETOOTH_GATT_* (GATT operations)
     * etc.
     */

    default:
        /* Not our message type */
        return -1;
    }
}

/**
 * List entities callback - exposes the BLE Scanning switch to Home Assistant
 */
static int bluetooth_proxy_list_entities(esphome_plugin_context_t *ctx, int client_id) {
    bluetooth_proxy_state_t *state = (bluetooth_proxy_state_t *)ctx->plugin_data;

    if (!state) {
        return -1;
    }

    /* Only expose the switch if we have a scanner */
    if (!state->scanner) {
        return 0;
    }

    /* Expose the BLE Scanning switch entity */
    list_entities_switch_response_t switch_entity = {
        .object_id = "ble_scanning",
        .key = ble_scanning_switch_key,
        .name = "BLE Scanning",
        .icon = "mdi:bluetooth-audio",
        .assumed_state = false,
        .disabled_by_default = false,
        .entity_category = 0,  /* None - normal entity */
        .device_class = ""
    };

    uint8_t encode_buf[512];
    size_t len = switch_encode_list_entities_response(encode_buf, sizeof(encode_buf), &switch_entity);

    if (len > 0) {
        esphome_plugin_send_message_to_client(ctx, client_id,
                                               ESPHOME_MSG_LIST_ENTITIES_SWITCH_RESPONSE,
                                               encode_buf, len);
        printf("[bluetooth_proxy] Registered BLE Scanning switch entity\n");
    } else {
        fprintf(stderr, "[bluetooth_proxy] Failed to encode switch entity\n");
        return -1;
    }

    return 0;
}

/**
 * Subscribe states callback - reports current BLE Scanning switch state
 */
static int bluetooth_proxy_subscribe_states(esphome_plugin_context_t *ctx, int client_id) {
    (void)client_id;  /* State is broadcast to all clients */

    bluetooth_proxy_state_t *state = (bluetooth_proxy_state_t *)ctx->plugin_data;

    if (!state || !state->scanner) {
        return 0;  /* No scanner, no state to report */
    }

    /* Report the current BLE scanning switch state */
    report_ble_scanning_switch_state(ctx, state->ble_scanning_enabled);

    return 0;
}

/**
 * Register the plugin
 *
 * This macro uses GCC constructor attribute to auto-register
 * the plugin when the binary loads.
 */
ESPHOME_PLUGIN_REGISTER(bluetooth_proxy_plugin, "BluetoothProxy", "1.0.0",
    bluetooth_proxy_init,
    bluetooth_proxy_cleanup,
    bluetooth_proxy_handle_message,
    bluetooth_proxy_configure_device_info,
    bluetooth_proxy_list_entities,
    bluetooth_proxy_subscribe_states
);
