/**
 * @file ble_scanner.h
 * @brief BLE Scanner using BlueZ D-Bus Interface
 *
 * Scans for BLE advertisements via BlueZ and forwards them
 * to the ESPHome API server for Home Assistant integration.
 */

#ifndef BLE_SCANNER_H
#define BLE_SCANNER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define BLE_MAC_LEN 6
#define BLE_ADV_DATA_MAX 62  /* BLE spec: 31 adv + 31 scan response */

/**
 * BLE advertisement data
 */
typedef struct {
    uint8_t address[BLE_MAC_LEN];  /* BLE MAC address */
    uint8_t address_type;          /* 0=public, 1=random */
    int8_t rssi;                   /* Signal strength in dBm */
    uint8_t data[BLE_ADV_DATA_MAX]; /* Combined advertisement data */
    size_t data_len;               /* Length of data */
} ble_advertisement_t;

/**
 * Callback for received BLE advertisements
 *
 * @param advert Advertisement data
 * @param user_data User-provided context
 */
typedef void (*ble_advert_callback_t)(const ble_advertisement_t *advert, void *user_data);

/**
 * BLE scanner instance (opaque)
 */
typedef struct ble_scanner ble_scanner_t;

/**
 * Initialize BLE scanner
 *
 * Connects to BlueZ via D-Bus system bus and sets up
 * monitoring for BLE advertisements.
 *
 * @param callback Callback function for advertisements
 * @param user_data User data passed to callback
 * @return Scanner instance, or NULL on error
 */
ble_scanner_t *ble_scanner_init(ble_advert_callback_t callback, void *user_data);

/**
 * Start BLE scanning
 *
 * Starts passive BLE scanning via BlueZ adapter.
 * Scans on all channels and reports all advertisements.
 *
 * @param scanner Scanner instance
 * @return 0 on success, -1 on error
 */
int ble_scanner_start(ble_scanner_t *scanner);

/**
 * Stop BLE scanning
 *
 * Stops scanning but keeps scanner initialized.
 *
 * @param scanner Scanner instance
 * @return 0 on success, -1 on error
 */
int ble_scanner_stop(ble_scanner_t *scanner);

/**
 * Check if scanner is running
 *
 * @param scanner Scanner instance
 * @return true if scanning, false otherwise
 */
bool ble_scanner_is_running(ble_scanner_t *scanner);

/**
 * Free BLE scanner
 *
 * Stops scanning and frees all resources.
 *
 * @param scanner Scanner instance
 */
void ble_scanner_free(ble_scanner_t *scanner);

#endif /* BLE_SCANNER_H */
