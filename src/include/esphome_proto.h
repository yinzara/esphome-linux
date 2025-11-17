/**
 * @file esphome_proto.h
 * @brief Minimal protobuf encoding/decoding for ESPHome Native API
 *
 * This is a lightweight implementation focused on the specific messages
 * needed for ESPHome Bluetooth Proxy functionality.
 */

#ifndef ESPHOME_PROTO_H
#define ESPHOME_PROTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ESPHome API Message Types */
#define ESPHOME_MSG_HELLO_REQUEST                                1
#define ESPHOME_MSG_HELLO_RESPONSE                               2
#define ESPHOME_MSG_CONNECT_REQUEST                              3
#define ESPHOME_MSG_CONNECT_RESPONSE                             4
#define ESPHOME_MSG_DISCONNECT_REQUEST                           5
#define ESPHOME_MSG_DISCONNECT_RESPONSE                          6
#define ESPHOME_MSG_PING_REQUEST                                 7
#define ESPHOME_MSG_PING_RESPONSE                                8
#define ESPHOME_MSG_DEVICE_INFO_REQUEST                          9
#define ESPHOME_MSG_DEVICE_INFO_RESPONSE                        10
#define ESPHOME_MSG_LIST_ENTITIES_REQUEST                       11
#define ESPHOME_MSG_LIST_ENTITIES_DONE_RESPONSE                 19
#define ESPHOME_MSG_SUBSCRIBE_STATES_REQUEST                    20
#define ESPHOME_MSG_SUBSCRIBE_HOMEASSISTANT_SERVICES_REQUEST    34
#define ESPHOME_MSG_SUBSCRIBE_HOMEASSISTANT_STATES_REQUEST      38

/* Entity State Messages (20-39) */
#define ESPHOME_MSG_BINARY_SENSOR_STATE_RESPONSE                 21
#define ESPHOME_MSG_COVER_STATE_RESPONSE                         22
#define ESPHOME_MSG_FAN_STATE_RESPONSE                           23
#define ESPHOME_MSG_LIGHT_STATE_RESPONSE                         24
#define ESPHOME_MSG_SENSOR_STATE_RESPONSE                        25
#define ESPHOME_MSG_SWITCH_STATE_RESPONSE                        26
#define ESPHOME_MSG_TEXT_SENSOR_STATE_RESPONSE                   27
#define ESPHOME_MSG_SUBSCRIBE_HOMEASSISTANT_SERVICES_REQUEST     34
#define ESPHOME_MSG_HOMEASSISTANT_SERVICE_RESPONSE               35

/* Entity Command Messages (30-33, 36-37, 40-65) */
#define ESPHOME_MSG_COVER_COMMAND_REQUEST                        30
#define ESPHOME_MSG_FAN_COMMAND_REQUEST                          31
#define ESPHOME_MSG_LIGHT_COMMAND_REQUEST                        32
#define ESPHOME_MSG_SWITCH_COMMAND_REQUEST                       33
#define ESPHOME_MSG_HOMEASSISTANT_SERVICE_CALL                   36
#define ESPHOME_MSG_HOMEASSISTANT_STATE_RESPONSE                 37
#define ESPHOME_MSG_CLIMATE_STATE_RESPONSE                       40
#define ESPHOME_MSG_CLIMATE_COMMAND_REQUEST                      41
#define ESPHOME_MSG_NUMBER_STATE_RESPONSE                        42
#define ESPHOME_MSG_NUMBER_COMMAND_REQUEST                       43
#define ESPHOME_MSG_SELECT_STATE_RESPONSE                        44
#define ESPHOME_MSG_SELECT_COMMAND_REQUEST                       45
#define ESPHOME_MSG_BUTTON_COMMAND_REQUEST                       46
#define ESPHOME_MSG_LOCK_STATE_RESPONSE                          47
#define ESPHOME_MSG_LOCK_COMMAND_REQUEST                         48
#define ESPHOME_MSG_VALVE_STATE_RESPONSE                         49
#define ESPHOME_MSG_VALVE_COMMAND_REQUEST                        50
#define ESPHOME_MSG_MEDIA_PLAYER_STATE_RESPONSE                  51
#define ESPHOME_MSG_MEDIA_PLAYER_COMMAND_REQUEST                 52
#define ESPHOME_MSG_ALARM_CONTROL_PANEL_STATE_RESPONSE           53
#define ESPHOME_MSG_ALARM_CONTROL_PANEL_COMMAND_REQUEST          54
#define ESPHOME_MSG_TEXT_STATE_RESPONSE                          55
#define ESPHOME_MSG_TEXT_COMMAND_REQUEST                         56
#define ESPHOME_MSG_DATE_STATE_RESPONSE                          57
#define ESPHOME_MSG_DATE_COMMAND_REQUEST                         58
#define ESPHOME_MSG_TIME_STATE_RESPONSE                          59
#define ESPHOME_MSG_TIME_COMMAND_REQUEST                         60
#define ESPHOME_MSG_DATETIME_STATE_RESPONSE                      61
#define ESPHOME_MSG_DATETIME_COMMAND_REQUEST                     62
#define ESPHOME_MSG_EVENT_RESPONSE                               63
#define ESPHOME_MSG_UPDATE_STATE_RESPONSE                        64
#define ESPHOME_MSG_UPDATE_COMMAND_REQUEST                       65

/* Bluetooth Proxy Messages (66-93) */
#define ESPHOME_MSG_SUBSCRIBE_BLUETOOTH_LE_ADVERTISEMENTS_REQUEST 66
#define ESPHOME_MSG_BLUETOOTH_LE_ADVERTISEMENT_RESPONSE           67
#define ESPHOME_MSG_BLUETOOTH_DEVICE_REQUEST                      68
#define ESPHOME_MSG_BLUETOOTH_DEVICE_CONNECTION_RESPONSE          69
#define ESPHOME_MSG_BLUETOOTH_GATT_GET_SERVICES_REQUEST           70
#define ESPHOME_MSG_BLUETOOTH_GATT_GET_SERVICES_RESPONSE          71
#define ESPHOME_MSG_BLUETOOTH_GATT_GET_SERVICES_DONE_RESPONSE     72
#define ESPHOME_MSG_BLUETOOTH_GATT_READ_REQUEST                   73
#define ESPHOME_MSG_BLUETOOTH_GATT_READ_RESPONSE                  74
#define ESPHOME_MSG_BLUETOOTH_GATT_WRITE_REQUEST                  75
#define ESPHOME_MSG_BLUETOOTH_GATT_READ_DESCRIPTOR_REQUEST        76
#define ESPHOME_MSG_BLUETOOTH_GATT_WRITE_DESCRIPTOR_REQUEST       77
#define ESPHOME_MSG_BLUETOOTH_GATT_NOTIFY_REQUEST                 78
#define ESPHOME_MSG_BLUETOOTH_GATT_NOTIFY_DATA_RESPONSE           79
#define ESPHOME_MSG_UNSUBSCRIBE_BLUETOOTH_LE_ADVERTISEMENTS_REQUEST 80
#define ESPHOME_MSG_BLUETOOTH_DEVICE_PAIRING_RESPONSE             81
#define ESPHOME_MSG_BLUETOOTH_DEVICE_UNPAIRING_RESPONSE           82
#define ESPHOME_MSG_BLUETOOTH_DEVICE_CLEAR_CACHE_RESPONSE         83
#define ESPHOME_MSG_SUBSCRIBE_BLUETOOTH_CONNECTIONS_FREE_REQUEST  84
#define ESPHOME_MSG_BLUETOOTH_CONNECTIONS_FREE_RESPONSE           85
#define ESPHOME_MSG_BLUETOOTH_GATT_GET_DESCRIPTOR_REQUEST         86
#define ESPHOME_MSG_BLUETOOTH_GATT_DESCRIPTOR_RESPONSE            87
#define ESPHOME_MSG_BLUETOOTH_GATT_NOTIFY_RESPONSE                88
#define ESPHOME_MSG_BLUETOOTH_GATT_ERROR_RESPONSE                 89
#define ESPHOME_MSG_BLUETOOTH_GATT_WRITE_RESPONSE                 90
#define ESPHOME_MSG_BLUETOOTH_GATT_WRITE_DESCRIPTOR_RESPONSE      91
#define ESPHOME_MSG_BLUETOOTH_LE_RAW_ADVERTISEMENTS_RESPONSE      93

/* Voice Assistant Messages (94-103) */
#define ESPHOME_MSG_SUBSCRIBE_VOICE_ASSISTANT_REQUEST             94
#define ESPHOME_MSG_VOICE_ASSISTANT_RESPONSE                      95
#define ESPHOME_MSG_VOICE_ASSISTANT_REQUEST                       96
#define ESPHOME_MSG_VOICE_ASSISTANT_AUDIO                         97
#define ESPHOME_MSG_VOICE_ASSISTANT_EVENT_RESPONSE                98
#define ESPHOME_MSG_VOICE_ASSISTANT_ANNOUNCE_REQUEST              99
#define ESPHOME_MSG_VOICE_ASSISTANT_ANNOUNCE_FINISHED            100
#define ESPHOME_MSG_VOICE_ASSISTANT_CONFIGURATION_REQUEST        101
#define ESPHOME_MSG_VOICE_ASSISTANT_CONFIGURATION_RESPONSE       102
#define ESPHOME_MSG_VOICE_ASSISTANT_TIMER_EVENT_RESPONSE         103

/* Maximum sizes */
#define ESPHOME_MAX_STRING_LEN     128
#define ESPHOME_MAX_ADV_DATA       62   /* BLE spec: 31 + 31 */
#define ESPHOME_MAX_ADV_BATCH      16
#define ESPHOME_MAX_MESSAGE_SIZE   4096

/* Protobuf wire types */
#define PB_WIRE_TYPE_VARINT    0
#define PB_WIRE_TYPE_64BIT     1
#define PB_WIRE_TYPE_LENGTH    2
#define PB_WIRE_TYPE_32BIT     5

/* Helper macros */
#define PB_FIELD_TAG(field_num, wire_type) (((field_num) << 3) | (wire_type))

/**
 * Protobuf encoder/decoder buffer
 */
typedef struct {
    uint8_t *data;      /* Buffer pointer */
    size_t size;        /* Buffer size */
    size_t pos;         /* Current position */
    bool error;         /* Error flag */
} pb_buffer_t;

/**
 * ESPHome message structures (minimal, only fields we use)
 */

typedef struct {
    char client[ESPHOME_MAX_STRING_LEN];
} esphome_hello_request_t;

typedef struct {
    uint32_t api_version_major;      /* Field 1 - uint32 */
    uint32_t api_version_minor;      /* Field 2 - uint32 */
    char server_info[ESPHOME_MAX_STRING_LEN];
    char name[ESPHOME_MAX_STRING_LEN];
} esphome_hello_response_t;

typedef struct {
    char password[ESPHOME_MAX_STRING_LEN];
} esphome_connect_request_t;

typedef struct {
    bool invalid_password;
} esphome_connect_response_t;

typedef struct {
    /* Empty */
} esphome_device_info_request_t;

/* Bluetooth Proxy Feature Flags (bitfield) */
#define BLE_FEATURE_PASSIVE_SCAN      (1 << 0)  /* Passive BLE scanning */
#define BLE_FEATURE_ACTIVE_SCAN       (1 << 1)  /* Active BLE scanning */
#define BLE_FEATURE_REMOTE_CACHE      (1 << 2)  /* Remote caching */
#define BLE_FEATURE_PAIRING           (1 << 3)  /* BLE pairing */
#define BLE_FEATURE_CACHE_CLEARING    (1 << 4)  /* Cache clearing */
#define BLE_FEATURE_RAW_ADVERTISEMENTS (1 << 5)  /* Raw advertisement data */

/* Voice Assistant Feature Flags (bitfield)
 * See api.proto VoiceAssistantFeature enum for complete list
 * TODO: Define specific flags when implementing voice_assistant plugin
 */
#define VOICE_ASSISTANT_FEATURE_VOICE_ASSISTANT (1 << 0)  /* Basic voice assistant support */
#define VOICE_ASSISTANT_FEATURE_SPEAKER        (1 << 1)  /* Speaker support */
#define VOICE_ASSISTANT_FEATURE_API_AUDIO      (1 << 2)  /* API audio streaming */
#define VOICE_ASSISTANT_FEATURE_TIMERS         (1 << 3)  /* Timer support */

/* Z-Wave Proxy Feature Flags (bitfield)
 * See api.proto ZWaveProxyFeature enum for complete list
 * TODO: Define specific flags when implementing zwave_proxy plugin
 */

typedef struct esphome_device_info_response {
    /* Core fields - match api.proto DeviceInfoResponse */
    bool uses_password;                      /* Field 1 */
    char name[ESPHOME_MAX_STRING_LEN];       /* Field 2 */
    char mac_address[24];                    /* Field 3 */
    char esphome_version[32];                /* Field 4 */
    char compilation_time[64];               /* Field 5 */
    char model[ESPHOME_MAX_STRING_LEN];      /* Field 6 */
    bool has_deep_sleep;                     /* Field 7 */
    char project_name[ESPHOME_MAX_STRING_LEN];    /* Field 8 */
    char project_version[ESPHOME_MAX_STRING_LEN]; /* Field 9 */
    uint32_t webserver_port;                 /* Field 10 */
    /* Note: Field 11 (legacy_bluetooth_proxy_version) deprecated - not included */
    char manufacturer[ESPHOME_MAX_STRING_LEN];    /* Field 12 */
    char friendly_name[ESPHOME_MAX_STRING_LEN];   /* Field 13 */
    /* Note: Field 14 (legacy_voice_assistant_version) deprecated - not included */
    uint32_t bluetooth_proxy_feature_flags;  /* Field 15 - BLE proxy capabilities */
    char suggested_area[64];                 /* Field 16 */
    uint32_t voice_assistant_feature_flags;  /* Field 17 - Voice assistant capabilities */
    char bluetooth_mac_address[24];          /* Field 18 - Bluetooth MAC address */
    bool api_encryption_supported;           /* Field 19 - API encryption support */
    /* Note: Fields 20-22 are repeated/nested messages - not yet implemented:
     *   20: devices (repeated DeviceInfo) - for multi-device proxies
     *   21: areas (repeated AreaInfo) - area definitions
     *   22: area (AreaInfo) - device's area
     */
    uint32_t zwave_proxy_feature_flags;      /* Field 23 - Z-Wave proxy capabilities */
    uint32_t zwave_home_id;                  /* Field 24 - Z-Wave network home ID */
} esphome_device_info_response_t;

typedef struct {
    /* Empty */
} esphome_list_entities_request_t;

typedef struct {
    /* Empty */
} esphome_list_entities_done_t;

typedef struct {
    uint32_t flags;
} esphome_subscribe_ble_advertisements_t;

typedef struct {
    uint64_t address;           /* BLE MAC address (little-endian uint64) */
    int32_t rssi;              /* Signal strength */
    uint32_t address_type;     /* 0=public, 1=random */
    uint8_t data[ESPHOME_MAX_ADV_DATA];
    size_t data_len;
} esphome_ble_advertisement_t;

typedef struct {
    esphome_ble_advertisement_t advertisements[ESPHOME_MAX_ADV_BATCH];
    size_t count;
} esphome_ble_advertisements_response_t;

/**
 * Protobuf encoding functions
 */

/* Initialize buffer for writing */
void pb_buffer_init_write(pb_buffer_t *buf, uint8_t *data, size_t size);

/* Initialize buffer for reading */
void pb_buffer_init_read(pb_buffer_t *buf, const uint8_t *data, size_t size);

/* Encode varint */
bool pb_encode_varint(pb_buffer_t *buf, uint64_t value);

/* Encode string */
bool pb_encode_string(pb_buffer_t *buf, uint32_t field_num, const char *str);

/* Encode bool */
bool pb_encode_bool(pb_buffer_t *buf, uint32_t field_num, bool value);

/* Encode fixed64 */
bool pb_encode_fixed64(pb_buffer_t *buf, uint32_t field_num, uint64_t value);

/* Encode uint64 (varint) */
bool pb_encode_uint64(pb_buffer_t *buf, uint32_t field_num, uint64_t value);

/* Encode sint32 (zigzag encoding) */
bool pb_encode_sint32(pb_buffer_t *buf, uint32_t field_num, int32_t value);

/* Encode uint32 */
bool pb_encode_uint32(pb_buffer_t *buf, uint32_t field_num, uint32_t value);

/* Encode bytes */
bool pb_encode_bytes(pb_buffer_t *buf, uint32_t field_num, const uint8_t *data, size_t len);

/* Decode varint */
bool pb_decode_varint(pb_buffer_t *buf, uint64_t *value);

/* Decode string */
bool pb_decode_string(pb_buffer_t *buf, char *str, size_t max_len);

/* Decode uint32 */
bool pb_decode_uint32(pb_buffer_t *buf, uint32_t *value);

/* Skip field */
bool pb_skip_field(pb_buffer_t *buf, uint8_t wire_type);

/**
 * ESPHome message encoding
 */

size_t esphome_encode_hello_response(uint8_t *buf, size_t size,
                                      const esphome_hello_response_t *msg);

size_t esphome_encode_connect_response(uint8_t *buf, size_t size,
                                        const esphome_connect_response_t *msg);

size_t esphome_encode_device_info_response(uint8_t *buf, size_t size,
                                            const esphome_device_info_response_t *msg);

size_t esphome_encode_list_entities_done(uint8_t *buf, size_t size);

size_t esphome_encode_ble_advertisements(uint8_t *buf, size_t size,
                                          const esphome_ble_advertisements_response_t *msg);

/**
 * ESPHome message decoding
 */

bool esphome_decode_hello_request(const uint8_t *buf, size_t size,
                                   esphome_hello_request_t *msg);

bool esphome_decode_connect_request(const uint8_t *buf, size_t size,
                                     esphome_connect_request_t *msg);

bool esphome_decode_subscribe_ble_advertisements(const uint8_t *buf, size_t size,
                                                  esphome_subscribe_ble_advertisements_t *msg);

/**
 * ESPHome message framing
 */

/* Encode message with framing (length + type + payload) */
size_t esphome_frame_message(uint8_t *out_buf, size_t out_size,
                              uint16_t msg_type,
                              const uint8_t *payload, size_t payload_len);

/* Decode message header (returns payload offset, or 0 on error) */
size_t esphome_decode_frame_header(const uint8_t *buf, size_t size,
                                    uint32_t *msg_len, uint16_t *msg_type);

#endif /* ESPHOME_PROTO_H */
