/**
 * @file ble_scanner.cpp
 * @brief BLE Scanner Implementation using libblepp BLEScanner
 */

#include "ble_scanner.h"
#include <blepp/lescan.h>
#include <blepp/bleclienttransport.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdexcept>

#define LOG_PREFIX "[ble-scanner] "

/**
 * Cached device state
 */
#define MAX_CACHED_DEVICES 64
#define REPORT_INTERVAL_MS 10000      /* Report every 10 seconds */
#define DEVICE_TIMEOUT_MS  60000      /* Remove devices not seen in 60 seconds */

typedef struct {
    uint8_t address[BLE_MAC_LEN];          /* BLE MAC address */
    uint8_t address_type;                   /* 0=public, 1=random */
    int8_t rssi;                            /* Signal strength */
    uint8_t data[BLE_ADV_DATA_MAX];        /* Advertisement data */
    size_t data_len;
    bool valid;
    uint64_t last_seen;                     /* Timestamp of last update */
} cached_device_t;

/**
 * BLE scanner instance
 */
struct ble_scanner {
    BLEPP::BLEClientTransport *transport;
    BLEPP::BLEScanner *scanner;
    ble_advert_callback_t callback;
    void *user_data;
    bool running;
    pthread_t event_thread;
    pthread_t report_thread;
    cached_device_t device_cache[MAX_CACHED_DEVICES];
    pthread_mutex_t cache_mutex;
    bool stop_requested;
};

/* -----------------------------------------------------------------
 * Utility functions
 * ----------------------------------------------------------------- */

/**
 * Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/**
 * Parse MAC address from string to bytes
 */
static bool parse_mac_address(const char *str, uint8_t *mac) {
    int values[6];
    if (sscanf(str, "%02X:%02X:%02X:%02X:%02X:%02X",
               &values[0], &values[1], &values[2],
               &values[3], &values[4], &values[5]) != 6) {
        // Try lowercase
        if (sscanf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
                   &values[0], &values[1], &values[2],
                   &values[3], &values[4], &values[5]) != 6) {
            return false;
        }
    }
    for (int i = 0; i < 6; i++) {
        mac[i] = (uint8_t)values[i];
    }
    return true;
}

/**
 * Find or create cached device entry
 */
static cached_device_t *get_or_create_cached_device(ble_scanner_t *scanner, const uint8_t *mac) {
    pthread_mutex_lock(&scanner->cache_mutex);

    /* Look for existing entry */
    for (int i = 0; i < MAX_CACHED_DEVICES; i++) {
        if (scanner->device_cache[i].valid &&
            memcmp(scanner->device_cache[i].address, mac, BLE_MAC_LEN) == 0) {
            pthread_mutex_unlock(&scanner->cache_mutex);
            return &scanner->device_cache[i];
        }
    }

    /* Find empty slot or oldest entry */
    cached_device_t *oldest = &scanner->device_cache[0];
    for (int i = 0; i < MAX_CACHED_DEVICES; i++) {
        if (!scanner->device_cache[i].valid) {
            /* Empty slot found */
            oldest = &scanner->device_cache[i];
            break;
        }
        if (scanner->device_cache[i].last_seen < oldest->last_seen) {
            oldest = &scanner->device_cache[i];
        }
    }

    /* Initialize new entry */
    memset(oldest, 0, sizeof(cached_device_t));
    memcpy(oldest->address, mac, BLE_MAC_LEN);
    oldest->valid = true;
    oldest->last_seen = get_timestamp_ms();

    pthread_mutex_unlock(&scanner->cache_mutex);
    return oldest;
}

/**
 * Remove stale devices from cache
 */
static void cleanup_stale_devices(ble_scanner_t *scanner) {
    uint64_t now = get_timestamp_ms();
    uint64_t timeout_threshold = now - DEVICE_TIMEOUT_MS;

    pthread_mutex_lock(&scanner->cache_mutex);

    int removed = 0;
    for (int i = 0; i < MAX_CACHED_DEVICES; i++) {
        cached_device_t *device = &scanner->device_cache[i];

        if (device->valid && device->last_seen < timeout_threshold) {
            printf(LOG_PREFIX "Removing stale device: %02X:%02X:%02X:%02X:%02X:%02X (not seen for %llu ms)\n",
                   device->address[0], device->address[1], device->address[2],
                   device->address[3], device->address[4], device->address[5],
                   (unsigned long long)(now - device->last_seen));
            memset(device, 0, sizeof(cached_device_t));
            removed++;
        }
    }

    pthread_mutex_unlock(&scanner->cache_mutex);

    if (removed > 0) {
        printf(LOG_PREFIX "Cleaned up %d stale device(s)\n", removed);
    }
}

/**
 * Convert libblepp advertisement to our format and merge into cache
 */
static void process_advertisement(ble_scanner_t *scanner, const BLEPP::AdvertisingResponse &ad) {
    uint8_t mac[BLE_MAC_LEN];
    if (!parse_mac_address(ad.address.c_str(), mac)) {
        printf(LOG_PREFIX "Failed to parse MAC address: %s\n", ad.address.c_str());
        return;
    }

    cached_device_t *device = get_or_create_cached_device(scanner, mac);

    pthread_mutex_lock(&scanner->cache_mutex);

    // Update RSSI
    device->rssi = ad.rssi;

    // Determine address type (random if contains local/private, public otherwise)
    // This is a simplification - libblepp doesn't directly expose address type
    device->address_type = 0; // Default to public

    // Build advertisement data from libblepp structures
    device->data_len = 0;

    // Add manufacturer data (type 0xFF)
    for (const auto &mfg : ad.manufacturer_specific_data) {
        if (device->data_len + 2 + mfg.size() <= BLE_ADV_DATA_MAX) {
            device->data[device->data_len++] = 1 + mfg.size(); // Length
            device->data[device->data_len++] = 0xFF;           // Type: Manufacturer Specific
            memcpy(&device->data[device->data_len], mfg.data(), mfg.size());
            device->data_len += mfg.size();
        }
    }

    // Add service data (type 0x16 for 16-bit UUIDs)
    for (const auto &svc : ad.service_data) {
        if (device->data_len + 2 + svc.size() <= BLE_ADV_DATA_MAX) {
            device->data[device->data_len++] = 1 + svc.size(); // Length
            device->data[device->data_len++] = 0x16;           // Type: Service Data - 16-bit UUID
            memcpy(&device->data[device->data_len], svc.data(), svc.size());
            device->data_len += svc.size();
        }
    }

    // Add UUIDs (type 0x03 for complete list of 16-bit UUIDs)
    if (!ad.UUIDs.empty() && ad.uuid_16_bit_complete) {
        size_t uuid_data_len = 0;
        uint8_t uuid_data[BLE_ADV_DATA_MAX];

        for (const auto &uuid : ad.UUIDs) {
            // Only include 16-bit UUIDs
            if (uuid.type == BLEPP::BT_UUID16 && uuid_data_len + 2 <= sizeof(uuid_data)) {
                uuid_data[uuid_data_len++] = uuid.value.u16 & 0xFF;
                uuid_data[uuid_data_len++] = (uuid.value.u16 >> 8) & 0xFF;
            }
        }

        if (uuid_data_len > 0 && device->data_len + 2 + uuid_data_len <= BLE_ADV_DATA_MAX) {
            device->data[device->data_len++] = 1 + uuid_data_len; // Length
            device->data[device->data_len++] = 0x03;              // Type: Complete 16-bit UUIDs
            memcpy(&device->data[device->data_len], uuid_data, uuid_data_len);
            device->data_len += uuid_data_len;
        }
    }

    // Add local name if present (type 0x08 for shortened, 0x09 for complete)
    if (ad.local_name != nullptr) {
        const std::string &name = ad.local_name->name;
        if (device->data_len + 2 + name.size() <= BLE_ADV_DATA_MAX) {
            device->data[device->data_len++] = 1 + name.size();                       // Length
            device->data[device->data_len++] = ad.local_name->complete ? 0x09 : 0x08; // Type
            memcpy(&device->data[device->data_len], name.c_str(), name.size());
            device->data_len += name.size();
        }
    }

    // Add flags if present (type 0x01)
    if (ad.flags != nullptr && !ad.flags->flag_data.empty()) {
        const auto &flags = ad.flags->flag_data;
        if (device->data_len + 2 + flags.size() <= BLE_ADV_DATA_MAX) {
            device->data[device->data_len++] = 1 + flags.size(); // Length
            device->data[device->data_len++] = 0x01;             // Type: Flags
            memcpy(&device->data[device->data_len], flags.data(), flags.size());
            device->data_len += flags.size();
        }
    }

    device->last_seen = get_timestamp_ms();

    pthread_mutex_unlock(&scanner->cache_mutex);
}

/* -----------------------------------------------------------------
 * Periodic reporting thread
 * ----------------------------------------------------------------- */

/**
 * Report thread - periodically reports all cached devices
 */
static void *report_thread_func(void *arg) {
    ble_scanner_t *scanner = (ble_scanner_t *)arg;

    printf(LOG_PREFIX "Report thread started\n");

    while (!scanner->stop_requested) {
        usleep(REPORT_INTERVAL_MS * 1000);

        if (scanner->stop_requested) {
            break;
        }

        /* Clean up stale devices first */
        cleanup_stale_devices(scanner);

        /* Report all active devices */
        pthread_mutex_lock(&scanner->cache_mutex);

        int reported = 0;
        for (int i = 0; i < MAX_CACHED_DEVICES; i++) {
            cached_device_t *device = &scanner->device_cache[i];

            if (!device->valid) {
                continue;
            }

            /* Create advertisement from cached state */
            ble_advertisement_t advert;
            memcpy(advert.address, device->address, sizeof(advert.address));
            advert.address_type = device->address_type;
            advert.rssi = device->rssi;
            memcpy(advert.data, device->data, device->data_len);
            advert.data_len = device->data_len;

            pthread_mutex_unlock(&scanner->cache_mutex);

            /* Send to callback */
            if (scanner->callback) {
                scanner->callback(&advert, scanner->user_data);
                reported++;
            }

            pthread_mutex_lock(&scanner->cache_mutex);
        }

        pthread_mutex_unlock(&scanner->cache_mutex);

        if (reported > 0) {
            printf(LOG_PREFIX "Reported %d device(s)\n", reported);
        }
    }

    printf(LOG_PREFIX "Report thread stopped\n");
    return NULL;
}

/* -----------------------------------------------------------------
 * Scanner event thread
 * ----------------------------------------------------------------- */

/**
 * Event loop thread - reads advertisements from BLEScanner
 */
static void *event_loop_thread(void *arg) {
    ble_scanner_t *scanner = (ble_scanner_t *)arg;

    printf(LOG_PREFIX "Event loop started\n");

    try {
        while (!scanner->stop_requested) {
            // Get advertisements from scanner (blocking call with timeout)
            std::vector<BLEPP::AdvertisingResponse> ads = scanner->scanner->get_advertisements(1000);

            for (const auto &ad : ads) {
                if (scanner->stop_requested) {
                    break;
                }

                // Process and cache the advertisement
                process_advertisement(scanner, ad);
            }
        }
    } catch (const std::exception &e) {
        fprintf(stderr, LOG_PREFIX "Scanner error: %s\n", e.what());
    }

    printf(LOG_PREFIX "Event loop stopped\n");
    return NULL;
}

/* -----------------------------------------------------------------
 * Public API (C linkage for plugin interface)
 * ----------------------------------------------------------------- */

ble_scanner_t *ble_scanner_init(ble_advert_callback_t callback, void *user_data) {
    ble_scanner_t *scanner = (ble_scanner_t *)calloc(1, sizeof(ble_scanner_t));
    if (!scanner) {
        fprintf(stderr, LOG_PREFIX "Failed to allocate scanner\n");
        return NULL;
    }

    scanner->callback = callback;
    scanner->user_data = user_data;
    scanner->running = false;
    scanner->stop_requested = false;

    /* Initialize device cache */
    memset(scanner->device_cache, 0, sizeof(scanner->device_cache));
    pthread_mutex_init(&scanner->cache_mutex, NULL);

    /* Set BLEPP log level from environment variable */
    const char *log_level_env = getenv("LOG_LEVEL");
    if (log_level_env != nullptr) {
        if (strcasecmp(log_level_env, "Debug") == 0) {
            BLEPP::log_level = BLEPP::Debug;
            printf(LOG_PREFIX "BLEPP log level set to Debug\n");
        } else if (strcasecmp(log_level_env, "Info") == 0) {
            BLEPP::log_level = BLEPP::Info;
            printf(LOG_PREFIX "BLEPP log level set to Info\n");
        } else if (strcasecmp(log_level_env, "Warning") == 0) {
            BLEPP::log_level = BLEPP::Warning;
            printf(LOG_PREFIX "BLEPP log level set to Warning\n");
        } else if (strcasecmp(log_level_env, "Error") == 0) {
            BLEPP::log_level = BLEPP::Error;
            printf(LOG_PREFIX "BLEPP log level set to Error\n");
        } else {
            printf(LOG_PREFIX "Unknown LOG_LEVEL '%s', valid values are Info, Debug, Warning and Error. using default (Info)\n", log_level_env);
            BLEPP::log_level = BLEPP::Info;
        }
    } else {
        /* Default to Info level if not set */
        BLEPP::log_level = BLEPP::Info;
    }

    /* Create BLE client transport */
    try {
        scanner->transport = BLEPP::create_client_transport();
        if (!scanner->transport) {
            fprintf(stderr, LOG_PREFIX "Failed to create BLE transport (no BlueZ or Nimble support)\n");
            pthread_mutex_destroy(&scanner->cache_mutex);
            free(scanner);
            return NULL;
        }

        printf(LOG_PREFIX "Using transport: %s\n", scanner->transport->get_transport_name());

        /* Create BLEScanner with the transport */
        scanner->scanner = new BLEPP::BLEScanner(
            scanner->transport,
            BLEPP::BLEScanner::FilterDuplicates::Software  /* Software duplicate filtering */
        );
    } catch (const std::exception &e) {
        fprintf(stderr, LOG_PREFIX "Failed to create BLEScanner: %s\n", e.what());
        if (scanner->transport) {
            delete scanner->transport;
        }
        pthread_mutex_destroy(&scanner->cache_mutex);
        free(scanner);
        return NULL;
    }

    printf(LOG_PREFIX "Scanner initialized\n");
    return scanner;
}

int ble_scanner_start(ble_scanner_t *scanner) {
    if (!scanner) {
        return -1;
    }

    if (scanner->running) {
        fprintf(stderr, LOG_PREFIX "Scanner already running\n");
        return -1;
    }

    scanner->stop_requested = false;

    /* Start BLE scanning (passive mode) */
    try {
        scanner->scanner->start(true);  /* true = passive scanning */
    } catch (const std::exception &e) {
        fprintf(stderr, LOG_PREFIX "Failed to start BLE scanner: %s\n", e.what());
        return -1;
    }

    /* Start event loop thread */
    scanner->running = true;
    if (pthread_create(&scanner->event_thread, NULL, event_loop_thread, scanner) != 0) {
        fprintf(stderr, LOG_PREFIX "Failed to create event thread\n");
        scanner->scanner->stop();
        scanner->running = false;
        return -1;
    }

    /* Start report thread */
    if (pthread_create(&scanner->report_thread, NULL, report_thread_func, scanner) != 0) {
        fprintf(stderr, LOG_PREFIX "Failed to create report thread\n");
        scanner->stop_requested = true;
        pthread_join(scanner->event_thread, NULL);
        scanner->scanner->stop();
        scanner->running = false;
        return -1;
    }

    printf(LOG_PREFIX "Scanner started (periodic reporting every %d ms)\n", REPORT_INTERVAL_MS);
    return 0;
}

int ble_scanner_stop(ble_scanner_t *scanner) {
    if (!scanner || !scanner->running) {
        return -1;
    }

    printf(LOG_PREFIX "Stopping scanner...\n");

    /* Signal threads to stop */
    scanner->stop_requested = true;

    /* Stop BLE scanner */
    try {
        scanner->scanner->stop();
    } catch (const std::exception &e) {
        fprintf(stderr, LOG_PREFIX "Error stopping BLE scanner: %s\n", e.what());
    }

    /* Wait for threads to finish */
    pthread_join(scanner->event_thread, NULL);
    pthread_join(scanner->report_thread, NULL);

    scanner->running = false;

    printf(LOG_PREFIX "Scanner stopped\n");
    return 0;
}

bool ble_scanner_is_running(ble_scanner_t *scanner) {
    return scanner && scanner->running;
}

void ble_scanner_free(ble_scanner_t *scanner) {
    if (!scanner) {
        return;
    }

    if (scanner->running) {
        ble_scanner_stop(scanner);
    }

    if (scanner->scanner) {
        delete scanner->scanner;
    }

    if (scanner->transport) {
        delete scanner->transport;
    }

    pthread_mutex_destroy(&scanner->cache_mutex);

    free(scanner);
    printf(LOG_PREFIX "Scanner freed\n");
}
