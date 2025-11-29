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
#include <stdexcept>

#define LOG_PREFIX "[ble-scanner] "

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
    bool stop_requested;
};

/* -----------------------------------------------------------------
 * Utility functions
 * ----------------------------------------------------------------- */

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
 * Convert libblepp advertisement to our format and report immediately
 */
static void process_advertisement(ble_scanner_t *scanner, const BLEPP::AdvertisingResponse &ad) {
    ble_advertisement_t advert;
    memset(&advert, 0, sizeof(advert));

    if (!parse_mac_address(ad.address.c_str(), advert.address)) {
        printf(LOG_PREFIX "Failed to parse MAC address: %s\n", ad.address.c_str());
        return;
    }

    advert.rssi = ad.rssi;

    // Determine address type (random if contains local/private, public otherwise)
    // This is a simplification - libblepp doesn't directly expose address type
    advert.address_type = 0; // Default to public

    // Use raw_packet data directly from libblepp - this contains the actual
    // advertisement data bytes as received from the BLE device
    advert.data_len = 0;

    for (const auto &packet : ad.raw_packet) {
        if (advert.data_len + packet.size() <= BLE_ADV_DATA_MAX) {
            memcpy(&advert.data[advert.data_len], packet.data(), packet.size());
            advert.data_len += packet.size();
        } else {
            // Would overflow, copy what we can
            size_t remaining = BLE_ADV_DATA_MAX - advert.data_len;
            if (remaining > 0) {
                memcpy(&advert.data[advert.data_len], packet.data(), remaining);
                advert.data_len = BLE_ADV_DATA_MAX;
            }
            break;
        }
    }

    // Report immediately via callback
    if (scanner->callback) {
        scanner->callback(&advert, scanner->user_data);
    }
}

/* -----------------------------------------------------------------
 * Scanner event thread
 * ----------------------------------------------------------------- */

/**
 * Event loop thread - reads advertisements from BLEScanner and reports immediately
 */
static void *event_loop_thread(void *arg) {
    ble_scanner_t *scanner = (ble_scanner_t *)arg;

    printf(LOG_PREFIX "Event loop started\n");

    try {
        while (!scanner->stop_requested) {
            // Get advertisements from scanner (blocking call with timeout)
            std::vector<BLEPP::AdvertisingResponse> ads = scanner->scanner->get_advertisements(100);

            for (const auto &ad : ads) {
                if (scanner->stop_requested) {
                    break;
                }

                // Process and report the advertisement immediately
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
            free(scanner);
            return NULL;
        }

        printf(LOG_PREFIX "Using transport: %s\n", scanner->transport->get_transport_name());

        /* Create BLEScanner with the transport - no duplicate filtering since we report immediately */
        scanner->scanner = new BLEPP::BLEScanner(
            scanner->transport
        );
    } catch (const std::exception &e) {
        fprintf(stderr, LOG_PREFIX "Failed to create BLEScanner: %s\n", e.what());
        if (scanner->transport) {
            delete scanner->transport;
        }
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

    /* Start BLE scanning (active mode) */
    try {
		BLEPP::ScanParams params;
		params.scan_type = BLEPP::ScanParams::ScanType::Active;
		params.interval_ms = 500;  // 500ms for WiFi coexistence
		params.window_ms = 60;      // 10% duty cycle (~50ms)
		params.filter_duplicates = BLEPP::ScanParams::FilterDuplicates::Off;
        scanner->scanner->start(params);  /* false = active scanning */
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

    printf(LOG_PREFIX "Scanner started (immediate reporting mode)\n");
    return 0;
}

int ble_scanner_stop(ble_scanner_t *scanner) {
    if (!scanner || !scanner->running) {
        return -1;
    }

    printf(LOG_PREFIX "Stopping scanner...\n");

    /* Signal thread to stop */
    scanner->stop_requested = true;

    /* Stop BLE scanner */
    try {
        scanner->scanner->stop();
    } catch (const std::exception &e) {
        fprintf(stderr, LOG_PREFIX "Error stopping BLE scanner: %s\n", e.what());
    }

    /* Wait for thread to finish */
    pthread_join(scanner->event_thread, NULL);

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

    free(scanner);
    printf(LOG_PREFIX "Scanner freed\n");
}
