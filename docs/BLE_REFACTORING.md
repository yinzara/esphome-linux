# BLE Batching Refactoring - Core to Plugin Migration

This document describes the refactoring that moved BLE advertisement batching from the core ESPHome API server into the bluetooth_proxy plugin.

## Overview

Previously, BLE advertisement batching was implemented in the core `esphome_api.c` file, making the core API server BLE-aware and coupled to Bluetooth-specific functionality. This refactoring moved all BLE batching logic into the bluetooth_proxy plugin, creating a cleaner separation of concerns.

## Motivation

1. **Separation of Concerns** - The core API server should handle generic ESPHome protocol operations, not BLE-specific batching
2. **Plugin Independence** - BLE functionality is optional and should be fully contained in the plugin
3. **Reusable API** - Other plugins need the same broadcast/send capabilities
4. **Maintainability** - BLE-specific code belongs with BLE-specific functionality

## What Was Moved

### From Core (`esphome_api.c`) to Plugin (`bluetooth_proxy_plugin.c`)

1. **Data Structures**
   - `esphome_ble_advertisements_response_t ble_batch` - Moved to plugin state
   - `pthread_mutex_t batch_mutex` - Moved to plugin state
   - `struct timespec last_flush` - Moved to plugin state
   - `pthread_t flush_thread` - Moved to plugin state
   - `bool flush_thread_running` - Moved to plugin state

2. **Functions**
   - `flush_ble_batch()` - Now in plugin, uses plugin messaging API
   - `flush_thread_func()` - Now in plugin
   - `esphome_api_queue_ble_advert()` - Removed from public API, logic in plugin's `on_ble_advertisement()`

3. **Thread Management**
   - Flush thread creation/destruction moved to plugin init/cleanup
   - Thread uses plugin state instead of server state

## New Generic API

The refactoring introduced a generic messaging API that any plugin can use:

### Core API Functions (esphome_api.h)

```c
/**
 * Send a message to a specific client
 */
int esphome_api_send_to_client(esphome_api_server_t *server,
                                int client_id,
                                uint16_t msg_type,
                                const uint8_t *payload,
                                size_t payload_len);

/**
 * Broadcast a message to all connected clients
 */
int esphome_api_broadcast(esphome_api_server_t *server,
                          uint16_t msg_type,
                          const uint8_t *payload,
                          size_t payload_len);
```

### Plugin API Wrappers (esphome_plugin.h)

```c
/**
 * Send a message to all connected clients
 */
int esphome_plugin_send_message(esphome_plugin_context_t *ctx,
                                uint32_t msg_type,
                                const uint8_t *data,
                                size_t len);

/**
 * Send a message to a specific client
 */
int esphome_plugin_send_message_to_client(esphome_plugin_context_t *ctx,
                                          int client_id,
                                          uint32_t msg_type,
                                          const uint8_t *data,
                                          size_t len);
```

## Changes by File

### `/src/include/esphome_api.h`

**Removed:**
```c
typedef struct {
    uint64_t address;
    int32_t rssi;
    uint32_t address_type;
    uint8_t data[31];
    size_t data_len;
} esphome_ble_advert_t;

int esphome_api_queue_ble_advert(esphome_api_server_t *server,
                                  const esphome_ble_advert_t *advert);
```

**Added:**
```c
int esphome_api_send_to_client(esphome_api_server_t *server,
                                int client_id,
                                uint16_t msg_type,
                                const uint8_t *payload,
                                size_t payload_len);

int esphome_api_broadcast(esphome_api_server_t *server,
                          uint16_t msg_type,
                          const uint8_t *payload,
                          size_t payload_len);
```

### `/src/esphome_api.c`

**Removed from `esphome_api_server_t`:**
```c
struct esphome_api_server {
    // ... other fields ...

    /* BLE batching - REMOVED */
    // esphome_ble_advertisements_response_t ble_batch;
    // pthread_mutex_t batch_mutex;
    // struct timespec last_flush;
    // pthread_t flush_thread;
    // bool flush_thread_running;
};
```

**Removed Functions:**
- `flush_ble_batch()` - Moved to plugin
- `flush_thread_func()` - Moved to plugin
- `esphome_api_queue_ble_advert()` - Moved to plugin

**Removed from `esphome_api_server_init()`:**
- BLE batch initialization
- Batch mutex initialization
- Flush thread creation

**Removed from `esphome_api_server_cleanup()`:**
- Flush thread termination
- Batch mutex destruction

**Added Functions:**
```c
int esphome_api_send_to_client(esphome_api_server_t *server,
                                int client_id,
                                uint16_t msg_type,
                                const uint8_t *payload,
                                size_t payload_len)
{
    if (client_id < 0 || client_id >= MAX_CLIENTS) {
        return -1;
    }

    esphome_api_client_t *client = &server->clients[client_id];
    pthread_mutex_lock(&client->mutex);

    if (client->state != CLIENT_AUTHENTICATED) {
        pthread_mutex_unlock(&client->mutex);
        return -1;
    }

    uint8_t frame[ESPHOME_MAX_MESSAGE_SIZE];
    size_t frame_len = esphome_frame_message(frame, sizeof(frame),
                                               msg_type, payload, payload_len);

    int result = send_all(client->fd, frame, frame_len);
    pthread_mutex_unlock(&client->mutex);

    return result;
}

int esphome_api_broadcast(esphome_api_server_t *server,
                          uint16_t msg_type,
                          const uint8_t *payload,
                          size_t payload_len)
{
    uint8_t frame[ESPHOME_MAX_MESSAGE_SIZE];
    size_t frame_len = esphome_frame_message(frame, sizeof(frame),
                                               msg_type, payload, payload_len);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        esphome_api_client_t *client = &server->clients[i];
        pthread_mutex_lock(&client->mutex);

        if (client->state == CLIENT_AUTHENTICATED) {
            send_all(client->fd, frame, frame_len);
        }

        pthread_mutex_unlock(&client->mutex);
    }

    return 0;
}
```

### `/src/esphome_plugin.c`

**Added Plugin Messaging Functions:**
```c
int esphome_plugin_send_message(esphome_plugin_context_t *ctx,
                                uint32_t msg_type,
                                const uint8_t *data,
                                size_t len)
{
    return esphome_api_broadcast(ctx->server, (uint16_t)msg_type, data, len);
}

int esphome_plugin_send_message_to_client(esphome_plugin_context_t *ctx,
                                          int client_id,
                                          uint32_t msg_type,
                                          const uint8_t *data,
                                          size_t len)
{
    return esphome_api_send_to_client(ctx->server, client_id,
                                      (uint16_t)msg_type, data, len);
}
```

### `/plugins/bluetooth_proxy/bluetooth_proxy_plugin.c`

**Added to Plugin State:**
```c
typedef struct {
    ble_scanner_t *scanner;
    bool subscribed;

    /* BLE advertisement batching - MOVED FROM CORE */
    esphome_ble_advertisements_response_t ble_batch;
    pthread_mutex_t batch_mutex;
    struct timespec last_flush;
    pthread_t flush_thread;
    bool flush_thread_running;

    /* Context reference for flush thread */
    esphome_plugin_context_t *ctx;
} bluetooth_proxy_state_t;
```

**Added Functions (moved from core):**
```c
static void flush_ble_batch(bluetooth_proxy_state_t *state)
{
    pthread_mutex_lock(&state->batch_mutex);

    if (state->ble_batch.count > 0) {
        uint8_t response_buf[ESPHOME_MAX_MESSAGE_SIZE];
        size_t response_len = esphome_encode_ble_advertisements(
            response_buf, sizeof(response_buf), &state->ble_batch);

        if (response_len > 0) {
            esphome_plugin_send_message(state->ctx,
                ESPHOME_MSG_BLUETOOTH_LE_ADVERTISEMENT_RESPONSE,
                response_buf, response_len);
        }

        state->ble_batch.count = 0;
        clock_gettime(CLOCK_MONOTONIC, &state->last_flush);
    }

    pthread_mutex_unlock(&state->batch_mutex);
}

static void *flush_thread_func(void *arg)
{
    bluetooth_proxy_state_t *state = (bluetooth_proxy_state_t *)arg;

    while (state->flush_thread_running) {
        usleep(100000);  // 100ms

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        double elapsed = (now.tv_sec - state->last_flush.tv_sec) +
                        (now.tv_nsec - state->last_flush.tv_nsec) / 1e9;

        if (elapsed >= 0.25) {  // 250ms
            flush_ble_batch(state);
        }
    }

    return NULL;
}

static void on_ble_advertisement(uint64_t address, int32_t rssi,
                                 const uint8_t *data, size_t data_len,
                                 void *user_data)
{
    bluetooth_proxy_state_t *state = (bluetooth_proxy_state_t *)user_data;

    if (!state->subscribed) {
        return;
    }

    pthread_mutex_lock(&state->batch_mutex);

    if (state->ble_batch.count >= ESPHOME_MAX_ADV_BATCH) {
        pthread_mutex_unlock(&state->batch_mutex);
        flush_ble_batch(state);
        pthread_mutex_lock(&state->batch_mutex);
    }

    esphome_ble_advertisement_t *adv =
        &state->ble_batch.advertisements[state->ble_batch.count];

    adv->address = address;
    adv->rssi = rssi;
    adv->address_type = 0;
    adv->data_len = data_len < ESPHOME_MAX_ADV_DATA ?
                    data_len : ESPHOME_MAX_ADV_DATA;
    memcpy(adv->data, data, adv->data_len);

    state->ble_batch.count++;

    pthread_mutex_unlock(&state->batch_mutex);
}
```

**Updated Plugin Lifecycle:**
```c
static int bluetooth_proxy_init(esphome_plugin_context_t *ctx)
{
    bluetooth_proxy_state_t *state = calloc(1, sizeof(*state));
    if (!state) {
        return -1;
    }

    /* Initialize batching */
    state->ble_batch.count = 0;
    pthread_mutex_init(&state->batch_mutex, NULL);
    clock_gettime(CLOCK_MONOTONIC, &state->last_flush);
    state->ctx = ctx;

    /* Start flush thread */
    state->flush_thread_running = true;
    if (pthread_create(&state->flush_thread, NULL,
                       flush_thread_func, state) != 0) {
        pthread_mutex_destroy(&state->batch_mutex);
        free(state);
        return -1;
    }

    /* Initialize scanner */
    state->scanner = ble_scanner_create();
    // ... rest of init ...
}

static void bluetooth_proxy_cleanup(esphome_plugin_context_t *ctx)
{
    bluetooth_proxy_state_t *state = ctx->plugin_data;

    /* Stop flush thread */
    state->flush_thread_running = false;
    pthread_join(state->flush_thread, NULL);

    /* Cleanup batching */
    pthread_mutex_destroy(&state->batch_mutex);

    /* Cleanup scanner */
    // ... rest of cleanup ...
}
```

## Benefits of This Refactoring

1. **Core Simplification**
   - Core API server is no longer BLE-aware
   - Reduced complexity in `esphome_api_server_t` structure
   - No BLE-specific threading in core

2. **Plugin Self-Containment**
   - BLE plugin manages its own batching state
   - BLE plugin manages its own flush thread
   - All BLE-specific logic in one place

3. **Reusability**
   - Generic `esphome_api_broadcast()` can be used by any plugin
   - Generic `esphome_api_send_to_client()` can be used by any plugin
   - Future plugins (Voice Assistant, Media Player, etc.) can use same API

4. **Flexibility**
   - Each plugin can implement its own batching strategy
   - Plugins can send messages whenever they need to
   - No assumptions about message patterns in core

5. **Conditional Compilation**
   - BLE code only compiled when bluetooth_proxy is enabled
   - Core remains BLE-free when feature is disabled
   - Smaller binary size for BLE-free builds

## Migration Guide for Other Plugins

If you're implementing a plugin that needs to batch messages, follow this pattern:

1. **Add batching state to your plugin state structure:**
```c
typedef struct {
    your_message_batch_t batch;
    pthread_mutex_t batch_mutex;
    struct timespec last_flush;
    pthread_t flush_thread;
    bool flush_thread_running;
    esphome_plugin_context_t *ctx;
} your_plugin_state_t;
```

2. **Implement flush function:**
```c
static void flush_batch(your_plugin_state_t *state)
{
    pthread_mutex_lock(&state->batch_mutex);

    if (state->batch.count > 0) {
        uint8_t buf[ESPHOME_MAX_MESSAGE_SIZE];
        size_t len = encode_your_batch(buf, sizeof(buf), &state->batch);

        if (len > 0) {
            esphome_plugin_send_message(state->ctx,
                YOUR_MESSAGE_TYPE, buf, len);
        }

        state->batch.count = 0;
        clock_gettime(CLOCK_MONOTONIC, &state->last_flush);
    }

    pthread_mutex_unlock(&state->batch_mutex);
}
```

3. **Create flush thread if needed:**
```c
static void *flush_thread_func(void *arg)
{
    your_plugin_state_t *state = arg;

    while (state->flush_thread_running) {
        usleep(YOUR_FLUSH_INTERVAL_US);

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        double elapsed = (now.tv_sec - state->last_flush.tv_sec) +
                        (now.tv_nsec - state->last_flush.tv_nsec) / 1e9;

        if (elapsed >= YOUR_FLUSH_THRESHOLD_SECONDS) {
            flush_batch(state);
        }
    }

    return NULL;
}
```

4. **Initialize in plugin init:**
```c
static int your_plugin_init(esphome_plugin_context_t *ctx)
{
    your_plugin_state_t *state = calloc(1, sizeof(*state));

    state->batch.count = 0;
    pthread_mutex_init(&state->batch_mutex, NULL);
    clock_gettime(CLOCK_MONOTONIC, &state->last_flush);
    state->ctx = ctx;

    state->flush_thread_running = true;
    pthread_create(&state->flush_thread, NULL, flush_thread_func, state);

    ctx->plugin_data = state;
    return 0;
}
```

5. **Cleanup in plugin cleanup:**
```c
static void your_plugin_cleanup(esphome_plugin_context_t *ctx)
{
    your_plugin_state_t *state = ctx->plugin_data;

    state->flush_thread_running = false;
    pthread_join(state->flush_thread, NULL);
    pthread_mutex_destroy(&state->batch_mutex);

    free(state);
}
```

## Testing

After this refactoring:

1. **BLE functionality remains unchanged** - Advertisements are still batched and flushed at 250ms intervals
2. **Performance is identical** - Same threading model, just moved to plugin
3. **API behavior is identical** - Clients see no difference in message patterns

The only difference is architectural - the implementation has moved from core to plugin.

## See Also

- [PLUGIN_ARCHITECTURE.md](PLUGIN_ARCHITECTURE.md) - Overview of plugin system
- [PLUGIN_DEVELOPMENT.md](PLUGIN_DEVELOPMENT.md) - Guide to developing plugins
- [PLUGIN_HOOKS.md](PLUGIN_HOOKS.md) - Documentation of plugin hooks
- [bluetooth_proxy_plugin.c](../plugins/bluetooth_proxy/bluetooth_proxy_plugin.c) - Reference implementation
