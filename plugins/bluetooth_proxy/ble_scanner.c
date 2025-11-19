/**
 * @file ble_scanner.c
 * @brief BLE Scanner Implementation using BlueZ D-Bus
 */

#include "ble_scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <dbus/dbus.h>
#include <glib.h>

#define LOG_PREFIX "[ble-scanner] "
#define BLUEZ_SERVICE "org.bluez"
#define BLUEZ_ADAPTER_INTERFACE "org.bluez.Adapter1"
#define BLUEZ_DEVICE_INTERFACE "org.bluez.Device1"
#define DBUS_PROPERTIES_INTERFACE "org.freedesktop.DBus.Properties"
#define ADAPTER_PATH "/org/bluez/hci0"

/**
 * D-Bus watch info
 */
typedef struct {
    DBusConnection *conn;
    guint source_id;
} dbus_watch_info_t;

/**
 * Cached device state
 */
#define MAX_CACHED_DEVICES 64
#define REPORT_INTERVAL_MS 10000      /* Report every 10 seconds */
#define DEVICE_TIMEOUT_MS  60000      /* Remove devices not seen in 60 seconds */

typedef struct {
    char path[128];              /* D-Bus object path */
    uint8_t address[6];          /* BLE MAC address */
    uint8_t address_type;        /* 0=public, 1=random */
    int8_t rssi;                 /* Signal strength */
    uint8_t data[BLE_ADV_DATA_MAX]; /* Advertisement data */
    size_t data_len;
    bool has_address;
    bool has_rssi;
    uint64_t last_seen;          /* Timestamp of last D-Bus update */
} cached_device_t;

/**
 * BLE scanner instance
 */
struct ble_scanner {
    DBusConnection *dbus_conn;
    ble_advert_callback_t callback;
    void *user_data;
    bool running;
    bool scanning;
    pthread_t event_thread;
    GMainLoop *main_loop;
    cached_device_t device_cache[MAX_CACHED_DEVICES];
    pthread_mutex_t cache_mutex;
    guint report_timer_id;       /* Periodic report timer */
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
 * Find or create cached device entry
 */
static cached_device_t *get_or_create_cached_device(ble_scanner_t *scanner, const char *path) {
    pthread_mutex_lock(&scanner->cache_mutex);

    /* Look for existing entry */
    for (int i = 0; i < MAX_CACHED_DEVICES; i++) {
        if (scanner->device_cache[i].path[0] != '\0' &&
            strcmp(scanner->device_cache[i].path, path) == 0) {
            pthread_mutex_unlock(&scanner->cache_mutex);
            return &scanner->device_cache[i];
        }
    }

    /* Find empty slot or oldest entry */
    cached_device_t *oldest = &scanner->device_cache[0];
    for (int i = 0; i < MAX_CACHED_DEVICES; i++) {
        if (scanner->device_cache[i].path[0] == '\0') {
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
    strncpy(oldest->path, path, sizeof(oldest->path) - 1);
    oldest->last_seen = get_timestamp_ms();

    pthread_mutex_unlock(&scanner->cache_mutex);
    return oldest;
}

/**
 * Parse MAC address from string to bytes
 */
static bool parse_mac_address(const char *str, uint8_t *mac) {
    int values[6];
    if (sscanf(str, "%02X:%02X:%02X:%02X:%02X:%02X",
               &values[0], &values[1], &values[2],
               &values[3], &values[4], &values[5]) != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) {
        mac[i] = (uint8_t)values[i];
    }
    return true;
}

/**
 * Parse MAC address from D-Bus object path
 * Path format: /org/bluez/hci0/dev_XX_XX_XX_XX_XX_XX
 */
static bool parse_mac_from_path(const char *path, uint8_t *mac) {
    const char *dev_prefix = "/org/bluez/hci0/dev_";
    const char *dev_part = strstr(path, dev_prefix);
    if (!dev_part) {
        printf(LOG_PREFIX "parse_mac_from_path: prefix not found in path: %s\n", path);
        return false;
    }

    /* Skip to the MAC part */
    const char *mac_str = dev_part + strlen(dev_prefix);
    printf(LOG_PREFIX "parse_mac_from_path: parsing MAC from: %s\n", mac_str);

    /* Parse MAC with underscores: XX_XX_XX_XX_XX_XX */
    int values[6];
    int parsed = sscanf(mac_str, "%02X_%02X_%02X_%02X_%02X_%02X",
                        &values[0], &values[1], &values[2],
                        &values[3], &values[4], &values[5]);

    printf(LOG_PREFIX "parse_mac_from_path: sscanf parsed %d fields\n", parsed);

    if (parsed != 6) {
        /* Try lowercase */
        parsed = sscanf(mac_str, "%02x_%02x_%02x_%02x_%02x_%02x",
                       &values[0], &values[1], &values[2],
                       &values[3], &values[4], &values[5]);
        printf(LOG_PREFIX "parse_mac_from_path: sscanf (lowercase) parsed %d fields\n", parsed);

        if (parsed != 6) {
            return false;
        }
    }

    for (int i = 0; i < 6; i++) {
        mac[i] = (uint8_t)values[i];
    }

    printf(LOG_PREFIX "parse_mac_from_path: successfully parsed MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return true;
}

/**
 * Parse address type from string
 */
static uint8_t parse_address_type(const char *type_str) {
    if (type_str && strcmp(type_str, "random") == 0) {
        return 1;
    }
    return 0; /* public */
}

/* -----------------------------------------------------------------
 * Advertisement data parsing
 * ----------------------------------------------------------------- */

/**
 * Append advertisement data element
 */
static void append_ad_element(uint8_t *data, size_t *data_len, size_t max_len,
                               uint8_t type, const uint8_t *value, size_t value_len) {
    if (*data_len + 2 + value_len > max_len) {
        return; /* No space */
    }

    data[(*data_len)++] = value_len + 1; /* Length */
    data[(*data_len)++] = type;          /* Type */
    memcpy(data + *data_len, value, value_len);
    *data_len += value_len;
}

/**
 * Parse ManufacturerData from D-Bus dict
 */
static void parse_manufacturer_data(DBusMessageIter *variant,
                                     uint8_t *data, size_t *data_len, size_t max_len) {
    DBusMessageIter dict, dict_entry;

    if (dbus_message_iter_get_arg_type(variant) != DBUS_TYPE_ARRAY) {
        return;
    }

    dbus_message_iter_recurse(variant, &dict);

    while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
        DBusMessageIter entry, value_variant, array;
        uint16_t company_id;

        dbus_message_iter_recurse(&dict, &entry);
        dbus_message_iter_get_basic(&entry, &company_id);
        dbus_message_iter_next(&entry);

        dbus_message_iter_recurse(&entry, &value_variant);

        if (dbus_message_iter_get_arg_type(&value_variant) != DBUS_TYPE_ARRAY) {
            dbus_message_iter_next(&dict);
            continue;
        }

        dbus_message_iter_recurse(&value_variant, &array);

        /* Build manufacturer data: company_id (LE) + data */
        uint8_t mfg_data[BLE_ADV_DATA_MAX];
        size_t mfg_len = 0;

        mfg_data[mfg_len++] = company_id & 0xFF;
        mfg_data[mfg_len++] = (company_id >> 8) & 0xFF;

        while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_BYTE) {
            uint8_t byte;
            dbus_message_iter_get_basic(&array, &byte);
            if (mfg_len < sizeof(mfg_data)) {
                mfg_data[mfg_len++] = byte;
            }
            dbus_message_iter_next(&array);
        }

        /* Append as type 0xFF (manufacturer specific data) */
        append_ad_element(data, data_len, max_len, 0xFF, mfg_data, mfg_len);

        dbus_message_iter_next(&dict);
    }
}

/**
 * Parse 16-bit UUID from Bluetooth Base UUID format
 * Format: 0000XXXX-0000-1000-8000-00805F9B34FB -> 0xXXXX
 */
static bool parse_uuid16(const char *uuid_str, uint16_t *uuid16) {
    /* Check for 16-bit UUID shorthand (4 hex chars) */
    if (strlen(uuid_str) == 4) {
        return sscanf(uuid_str, "%04hx", uuid16) == 1;
    }

    /* Check for full Bluetooth Base UUID format */
    if (strlen(uuid_str) >= 8 && uuid_str[8] == '-') {
        /* Extract XXXX from 0000XXXX-0000-1000-8000-00805F9B34FB */
        return sscanf(uuid_str + 4, "%04hx", uuid16) == 1;
    }

    return false;
}

/**
 * Parse ServiceData from D-Bus dict
 */
static void parse_service_data(DBusMessageIter *variant,
                                uint8_t *data, size_t *data_len, size_t max_len) {
    DBusMessageIter dict, dict_entry;

    if (dbus_message_iter_get_arg_type(variant) != DBUS_TYPE_ARRAY) {
        return;
    }

    dbus_message_iter_recurse(variant, &dict);

    while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
        DBusMessageIter entry, value_variant, array;
        const char *uuid;

        dbus_message_iter_recurse(&dict, &entry);
        dbus_message_iter_get_basic(&entry, &uuid);
        dbus_message_iter_next(&entry);

        dbus_message_iter_recurse(&entry, &value_variant);

        if (dbus_message_iter_get_arg_type(&value_variant) != DBUS_TYPE_ARRAY) {
            dbus_message_iter_next(&dict);
            continue;
        }

        dbus_message_iter_recurse(&value_variant, &array);

        uint8_t svc_data[BLE_ADV_DATA_MAX];
        size_t svc_len = 0;

        /* Parse UUID - extract 16-bit UUID from full or short format */
        uint16_t uuid16 = 0;
        if (parse_uuid16(uuid, &uuid16)) {
            /* UUID in little-endian */
            svc_data[svc_len++] = uuid16 & 0xFF;
            svc_data[svc_len++] = (uuid16 >> 8) & 0xFF;
        }

        while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_BYTE) {
            uint8_t byte;
            dbus_message_iter_get_basic(&array, &byte);
            if (svc_len < sizeof(svc_data)) {
                svc_data[svc_len++] = byte;
            }
            dbus_message_iter_next(&array);
        }

        /* Append as type 0x16 (service data - 16-bit UUID) */
        append_ad_element(data, data_len, max_len, 0x16, svc_data, svc_len);

        dbus_message_iter_next(&dict);
    }
}

/**
 * Parse ServiceUUIDs from D-Bus array
 */
static void parse_service_uuids(DBusMessageIter *variant,
                                 uint8_t *data, size_t *data_len, size_t max_len) {
    DBusMessageIter array;

    if (dbus_message_iter_get_arg_type(variant) != DBUS_TYPE_ARRAY) {
        return;
    }

    dbus_message_iter_recurse(variant, &array);

    uint8_t uuid_list[BLE_ADV_DATA_MAX];
    size_t uuid_len = 0;

    while (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_STRING) {
        const char *uuid;
        dbus_message_iter_get_basic(&array, &uuid);

        /* Parse 16-bit UUID from string */
        uint16_t uuid16 = 0;
        if (parse_uuid16(uuid, &uuid16)) {
            if (uuid_len + 2 <= sizeof(uuid_list)) {
                /* UUID in little-endian */
                uuid_list[uuid_len++] = uuid16 & 0xFF;
                uuid_list[uuid_len++] = (uuid16 >> 8) & 0xFF;
            }
        }

        dbus_message_iter_next(&array);
    }

    if (uuid_len > 0) {
        /* Append as type 0x03 (complete list of 16-bit service UUIDs) */
        append_ad_element(data, data_len, max_len, 0x03, uuid_list, uuid_len);
    }
}

/* -----------------------------------------------------------------
 * Periodic reporting
 * ----------------------------------------------------------------- */

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

        if (device->path[0] != '\0' && device->last_seen < timeout_threshold) {
            printf(LOG_PREFIX "Removing stale device: %s (not seen for %llu ms)\n",
                   device->path, (unsigned long long)(now - device->last_seen));
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
 * Periodic report timer callback
 */
static gboolean report_timer_callback(gpointer user_data) {
    ble_scanner_t *scanner = (ble_scanner_t *)user_data;

    if (!scanner || !scanner->running) {
        return FALSE;  /* Stop timer */
    }

    /* Clean up stale devices first */
    cleanup_stale_devices(scanner);

    /* Report all active devices */
    pthread_mutex_lock(&scanner->cache_mutex);

    int reported = 0;
    for (int i = 0; i < MAX_CACHED_DEVICES; i++) {
        cached_device_t *device = &scanner->device_cache[i];

        /* Skip empty slots and devices without complete data */
        if (device->path[0] == '\0' || !device->has_address || !device->has_rssi) {
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

    return TRUE;  /* Continue timer */
}

/* -----------------------------------------------------------------
 * BlueZ D-Bus signal handling
 * ----------------------------------------------------------------- */

/**
 * Handle PropertiesChanged signal from BlueZ devices
 */
static DBusHandlerResult properties_changed_filter(DBusConnection *conn,
                                                    DBusMessage *msg,
                                                    void *user_data) {
    ble_scanner_t *scanner = (ble_scanner_t *)user_data;

    if (!dbus_message_is_signal(msg, DBUS_PROPERTIES_INTERFACE, "PropertiesChanged")) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    const char *path = dbus_message_get_path(msg);
    if (!path || strstr(path, "/org/bluez/hci0/dev_") == NULL) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    printf(LOG_PREFIX "Received PropertiesChanged for %s\n", path);

    DBusMessageIter iter, dict;
    const char *interface;

    dbus_message_iter_init(msg, &iter);
    dbus_message_iter_get_basic(&iter, &interface);

    if (strcmp(interface, BLUEZ_DEVICE_INTERFACE) != 0) {
        printf(LOG_PREFIX "Ignoring non-Device1 interface: %s\n", interface);
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    dbus_message_iter_next(&iter);
    dbus_message_iter_recurse(&iter, &dict);

    /* Get or create cached device */
    cached_device_t *device = get_or_create_cached_device(scanner, path);

    /* Update cached address from path if not set */
    if (!device->has_address) {
        if (parse_mac_from_path(path, device->address)) {
            device->has_address = true;
        }
    }

    /* Parse property updates and merge into cache */
    pthread_mutex_lock(&scanner->cache_mutex);

    bool updated = false;
    bool rssi_updated = false;

    while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
        DBusMessageIter entry, variant;
        const char *key;

        dbus_message_iter_recurse(&dict, &entry);
        dbus_message_iter_get_basic(&entry, &key);
        dbus_message_iter_next(&entry);
        dbus_message_iter_recurse(&entry, &variant);

        printf(LOG_PREFIX "Property changed: %s\n", key);

        if (strcmp(key, "Address") == 0) {
            const char *addr_str;
            dbus_message_iter_get_basic(&variant, &addr_str);
            if (parse_mac_address(addr_str, device->address)) {
                device->has_address = true;
                updated = true;
            }
        } else if (strcmp(key, "AddressType") == 0) {
            const char *type_str;
            dbus_message_iter_get_basic(&variant, &type_str);
            device->address_type = parse_address_type(type_str);
            updated = true;
        } else if (strcmp(key, "RSSI") == 0) {
            int16_t rssi;
            dbus_message_iter_get_basic(&variant, &rssi);
            device->rssi = (int8_t)rssi;
            device->has_rssi = true;
            rssi_updated = true;
            updated = true;
        } else if (strcmp(key, "ManufacturerData") == 0) {
            /* Reset data and parse new manufacturer data */
            device->data_len = 0;
            parse_manufacturer_data(&variant, device->data, &device->data_len,
                                   sizeof(device->data));
            updated = true;
        } else if (strcmp(key, "ServiceData") == 0) {
            /* Append service data */
            parse_service_data(&variant, device->data, &device->data_len,
                              sizeof(device->data));
            updated = true;
        } else if (strcmp(key, "ServiceUUIDs") == 0) {
            /* Append service UUIDs */
            parse_service_uuids(&variant, device->data, &device->data_len,
                               sizeof(device->data));
            updated = true;
        }

        dbus_message_iter_next(&dict);
    }

    /* Update last seen timestamp */
    device->last_seen = get_timestamp_ms();

    pthread_mutex_unlock(&scanner->cache_mutex);

    /* Devices will be reported by the periodic timer, not immediately */

    return DBUS_HANDLER_RESULT_HANDLED;
}

/* -----------------------------------------------------------------
 * D-Bus / GLib integration
 * ----------------------------------------------------------------- */

/**
 * Free watch info
 */
static void free_watch_info(void *data) {
    dbus_watch_info_t *info = (dbus_watch_info_t *)data;
    if (info) {
        free(info);
    }
}

/**
 * Handle D-Bus watch events
 */
static gboolean dbus_watch_handler(GIOChannel *channel, GIOCondition condition, gpointer data) {
    DBusWatch *watch = (DBusWatch *)data;
    dbus_watch_info_t *info = (dbus_watch_info_t *)dbus_watch_get_data(watch);
    unsigned int flags = 0;

    (void)channel;

    if (!info) {
        return FALSE;
    }

    if (condition & G_IO_IN)  flags |= DBUS_WATCH_READABLE;
    if (condition & G_IO_OUT) flags |= DBUS_WATCH_WRITABLE;
    if (condition & G_IO_ERR) flags |= DBUS_WATCH_ERROR;
    if (condition & G_IO_HUP) flags |= DBUS_WATCH_HANGUP;

    /* Let D-Bus handle the watch */
    dbus_watch_handle(watch, flags);

    /* Dispatch all pending messages */
    while (info->conn && dbus_connection_dispatch(info->conn) == DBUS_DISPATCH_DATA_REMAINS) {
        /* Keep dispatching */
    }

    return TRUE;
}

/**
 * Add D-Bus watch callback
 */
static dbus_bool_t add_watch(DBusWatch *watch, void *data) {
    ble_scanner_t *scanner = (ble_scanner_t *)data;

    if (!dbus_watch_get_enabled(watch)) {
        return TRUE;
    }

    int fd = dbus_watch_get_unix_fd(watch);
    unsigned int flags = dbus_watch_get_flags(watch);

    GIOCondition condition = G_IO_ERR | G_IO_HUP;
    if (flags & DBUS_WATCH_READABLE) condition |= G_IO_IN;
    if (flags & DBUS_WATCH_WRITABLE) condition |= G_IO_OUT;

    GIOChannel *channel = g_io_channel_unix_new(fd);
    guint source = g_io_add_watch(channel, condition, dbus_watch_handler, watch);
    g_io_channel_unref(channel);

    if (source == 0) {
        return FALSE;
    }

    /* Allocate watch info */
    dbus_watch_info_t *info = malloc(sizeof(dbus_watch_info_t));
    if (!info) {
        g_source_remove(source);
        return FALSE;
    }

    info->conn = scanner->dbus_conn;
    info->source_id = source;

    /* Store info in watch data */
    dbus_watch_set_data(watch, info, free_watch_info);

    return TRUE;
}

/**
 * Remove D-Bus watch callback
 */
static void remove_watch(DBusWatch *watch, void *data) {
    (void)data;

    dbus_watch_info_t *info = (dbus_watch_info_t *)dbus_watch_get_data(watch);
    if (info && info->source_id != 0) {
        g_source_remove(info->source_id);
        info->source_id = 0;
    }
}

/**
 * Toggle D-Bus watch callback
 */
static void toggle_watch(DBusWatch *watch, void *data) {
    if (dbus_watch_get_enabled(watch)) {
        add_watch(watch, data);
    } else {
        remove_watch(watch, data);
    }
}

/**
 * D-Bus timeout callback
 */
static gboolean timeout_handler(gpointer data) {
    DBusTimeout *timeout = (DBusTimeout *)data;
    dbus_timeout_handle(timeout);
    return FALSE; /* Don't repeat */
}

/**
 * Add D-Bus timeout callback
 */
static dbus_bool_t add_timeout(DBusTimeout *timeout, void *data) {
    (void)data;

    if (!dbus_timeout_get_enabled(timeout)) {
        return TRUE;
    }

    int interval = dbus_timeout_get_interval(timeout);
    guint source = g_timeout_add(interval, timeout_handler, timeout);

    if (source == 0) {
        return FALSE;
    }

    dbus_timeout_set_data(timeout, GUINT_TO_POINTER(source), NULL);
    return TRUE;
}

/**
 * Remove D-Bus timeout callback
 */
static void remove_timeout(DBusTimeout *timeout, void *data) {
    (void)data;

    guint source = GPOINTER_TO_UINT(dbus_timeout_get_data(timeout));
    if (source != 0) {
        g_source_remove(source);
    }
}

/**
 * Toggle D-Bus timeout callback
 */
static void toggle_timeout(DBusTimeout *timeout, void *data) {
    if (dbus_timeout_get_enabled(timeout)) {
        add_timeout(timeout, data);
    } else {
        remove_timeout(timeout, data);
    }
}

/**
 * D-Bus wakeup callback
 */
static void wakeup_main(void *data) {
    ble_scanner_t *scanner = (ble_scanner_t *)data;

    /* Dispatch queued messages */
    while (dbus_connection_dispatch(scanner->dbus_conn) == DBUS_DISPATCH_DATA_REMAINS) {
        /* Keep dispatching */
    }
}

/**
 * Setup D-Bus connection with GLib main loop
 */
static bool setup_dbus_glib_integration(ble_scanner_t *scanner) {
    /* Set up watch functions */
    if (!dbus_connection_set_watch_functions(scanner->dbus_conn,
                                             add_watch,
                                             remove_watch,
                                             toggle_watch,
                                             scanner,
                                             NULL)) {
        fprintf(stderr, LOG_PREFIX "Failed to set D-Bus watch functions\n");
        return false;
    }

    /* Set up timeout functions */
    if (!dbus_connection_set_timeout_functions(scanner->dbus_conn,
                                               add_timeout,
                                               remove_timeout,
                                               toggle_timeout,
                                               scanner,
                                               NULL)) {
        fprintf(stderr, LOG_PREFIX "Failed to set D-Bus timeout functions\n");
        return false;
    }

    /* Set wakeup function */
    dbus_connection_set_wakeup_main_function(scanner->dbus_conn,
                                             wakeup_main,
                                             scanner,
                                             NULL);

    printf(LOG_PREFIX "D-Bus/GLib integration configured\n");
    return true;
}

/* -----------------------------------------------------------------
 * Scanner control
 * ----------------------------------------------------------------- */

/**
 * Start discovery on BlueZ adapter
 */
static int start_discovery(ble_scanner_t *scanner) {
    DBusMessage *msg;
    DBusMessage *reply;
    DBusError error;

    dbus_error_init(&error);

    msg = dbus_message_new_method_call(
        BLUEZ_SERVICE,
        ADAPTER_PATH,
        BLUEZ_ADAPTER_INTERFACE,
        "StartDiscovery"
    );

    if (!msg) {
        fprintf(stderr, LOG_PREFIX "Failed to create StartDiscovery message\n");
        return -1;
    }

    reply = dbus_connection_send_with_reply_and_block(scanner->dbus_conn, msg, 5000, &error);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&error)) {
        /* If already discovering, that's okay */
        if (strcmp(error.name, "org.bluez.Error.InProgress") == 0) {
            fprintf(stderr, LOG_PREFIX "Discovery already in progress\n");
            dbus_error_free(&error);
            return 0;
        }

        fprintf(stderr, LOG_PREFIX "StartDiscovery failed: %s\n", error.message);
        dbus_error_free(&error);
        return -1;
    }

    if (reply) {
        dbus_message_unref(reply);
    }

    printf(LOG_PREFIX "BLE discovery started\n");
    scanner->scanning = true;
    return 0;
}

/**
 * Stop discovery on BlueZ adapter
 */
static int stop_discovery(ble_scanner_t *scanner) {
    DBusMessage *msg;
    DBusMessage *reply;
    DBusError error;

    if (!scanner->scanning) {
        return 0;
    }

    dbus_error_init(&error);

    msg = dbus_message_new_method_call(
        BLUEZ_SERVICE,
        ADAPTER_PATH,
        BLUEZ_ADAPTER_INTERFACE,
        "StopDiscovery"
    );

    if (!msg) {
        fprintf(stderr, LOG_PREFIX "Failed to create StopDiscovery message\n");
        return -1;
    }

    reply = dbus_connection_send_with_reply_and_block(scanner->dbus_conn, msg, 5000, &error);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&error)) {
        fprintf(stderr, LOG_PREFIX "StopDiscovery failed: %s\n", error.message);
        dbus_error_free(&error);
        return -1;
    }

    if (reply) {
        dbus_message_unref(reply);
    }

    printf(LOG_PREFIX "BLE discovery stopped\n");
    scanner->scanning = false;
    return 0;
}

/**
 * Event loop thread
 */
static void *event_loop_thread(void *arg) {
    ble_scanner_t *scanner = (ble_scanner_t *)arg;

    printf(LOG_PREFIX "Event loop started\n");
    g_main_loop_run(scanner->main_loop);
    printf(LOG_PREFIX "Event loop stopped\n");

    return NULL;
}

/* -----------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------- */

ble_scanner_t *ble_scanner_init(ble_advert_callback_t callback, void *user_data) {
    DBusError error;
    dbus_error_init(&error);

    ble_scanner_t *scanner = calloc(1, sizeof(ble_scanner_t));
    if (!scanner) {
        fprintf(stderr, LOG_PREFIX "Failed to allocate scanner\n");
        return NULL;
    }

    scanner->callback = callback;
    scanner->user_data = user_data;
    scanner->running = false;
    scanner->scanning = false;

    /* Initialize device cache */
    memset(scanner->device_cache, 0, sizeof(scanner->device_cache));
    pthread_mutex_init(&scanner->cache_mutex, NULL);

    /* Create GLib main loop first */
    scanner->main_loop = g_main_loop_new(NULL, FALSE);

    /* Connect to D-Bus system bus */
    scanner->dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    if (dbus_error_is_set(&error)) {
        fprintf(stderr, LOG_PREFIX "D-Bus connection error: %s\n", error.message);
        dbus_error_free(&error);
        g_main_loop_unref(scanner->main_loop);
        free(scanner);
        return NULL;
    }

    /* Integrate D-Bus connection with GLib main loop */
    dbus_connection_set_exit_on_disconnect(scanner->dbus_conn, FALSE);

    if (!setup_dbus_glib_integration(scanner)) {
        fprintf(stderr, LOG_PREFIX "Failed to setup D-Bus/GLib integration\n");
        dbus_connection_unref(scanner->dbus_conn);
        g_main_loop_unref(scanner->main_loop);
        free(scanner);
        return NULL;
    }

    /* Add message filter for PropertiesChanged signals */
    dbus_bus_add_match(scanner->dbus_conn,
                      "type='signal',interface='org.freedesktop.DBus.Properties'",
                      &error);
    if (dbus_error_is_set(&error)) {
        fprintf(stderr, LOG_PREFIX "Failed to add match: %s\n", error.message);
        dbus_error_free(&error);
        dbus_connection_unref(scanner->dbus_conn);
        free(scanner);
        return NULL;
    }

    dbus_connection_add_filter(scanner->dbus_conn, properties_changed_filter, scanner, NULL);

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

    /* Start event loop thread */
    scanner->running = true;
    if (pthread_create(&scanner->event_thread, NULL, event_loop_thread, scanner) != 0) {
        fprintf(stderr, LOG_PREFIX "Failed to create event thread\n");
        scanner->running = false;
        return -1;
    }

    /* Give the event loop time to start */
    usleep(100000); /* 100ms */

    /* Start BLE discovery */
    if (start_discovery(scanner) < 0) {
        scanner->running = false;
        g_main_loop_quit(scanner->main_loop);
        pthread_join(scanner->event_thread, NULL);
        return -1;
    }

    /* Start periodic reporting timer */
    scanner->report_timer_id = g_timeout_add(REPORT_INTERVAL_MS, report_timer_callback, scanner);
    printf(LOG_PREFIX "Periodic reporting timer started (interval: %d ms)\n", REPORT_INTERVAL_MS);

    printf(LOG_PREFIX "Scanner started\n");
    return 0;
}

int ble_scanner_stop(ble_scanner_t *scanner) {
    if (!scanner || !scanner->running) {
        return -1;
    }

    /* Stop discovery */
    stop_discovery(scanner);

    /* Stop periodic reporting timer */
    if (scanner->report_timer_id > 0) {
        g_source_remove(scanner->report_timer_id);
        scanner->report_timer_id = 0;
        printf(LOG_PREFIX "Periodic reporting timer stopped\n");
    }

    /* Stop event loop */
    scanner->running = false;
    g_main_loop_quit(scanner->main_loop);
    pthread_join(scanner->event_thread, NULL);

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

    if (scanner->main_loop) {
        g_main_loop_unref(scanner->main_loop);
    }

    if (scanner->dbus_conn) {
        /* Remove the filter we added during init */
        dbus_connection_remove_filter(scanner->dbus_conn, properties_changed_filter, scanner);

        /* Remove the match rule we added during init */
        DBusError error;
        dbus_error_init(&error);
        dbus_bus_remove_match(scanner->dbus_conn,
                              "type='signal',interface='org.freedesktop.DBus.Properties'",
                              &error);
        if (dbus_error_is_set(&error)) {
            fprintf(stderr, LOG_PREFIX "Failed to remove match: %s\n", error.message);
            dbus_error_free(&error);
        }

        /* Flush any pending outgoing messages */
        dbus_connection_flush(scanner->dbus_conn);

        /* Now we can safely unref the connection */
        dbus_connection_unref(scanner->dbus_conn);
    }

    pthread_mutex_destroy(&scanner->cache_mutex);

    free(scanner);
    printf(LOG_PREFIX "Scanner freed\n");
}
