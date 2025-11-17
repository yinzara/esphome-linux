# Plugin Development Guide

This guide explains how to create plugins to extend the esphome-linux-proxy with additional ESPHome Native API functionality.

## Table of Contents

1. [Overview](#overview)
2. [Plugin Architecture](#plugin-architecture)
3. [Creating Your First Plugin](#creating-your-first-plugin)
4. [Protobuf Message Handling](#protobuf-message-handling)
5. [Plugin API Reference](#plugin-api-reference)
6. [Building with Plugins](#building-with-plugins)
7. [Deployment](#deployment)
8. [Examples](#examples)
9. [Best Practices](#best-practices)
10. [Troubleshooting](#troubleshooting)
11. [Resources](#resources)

## Overview

The plugin system allows you to extend esphome-linux-proxy with additional ESPHome entities and functionality without modifying the core codebase. This is useful for:

- Adding MediaPlayer support with ALSA/PulseAudio
- Implementing VoiceAssistant with microphone/speaker
- Adding Climate control for HVAC systems
- Creating custom sensors or switches
- Implementing Cover/Blinds control
- Any other ESPHome entity type

### Key Features

- **Simple C API** - Easy-to-use callback-based interface
- **Auto-registration** - Plugins automatically register using GCC constructors
- **Message routing** - Handle custom ESPHome Native API messages
- **Bidirectional communication** - Send and receive messages
- **Buildroot integration** - Multiple deployment options
- **No core modifications** - Extend without changing base code

## Plugin Architecture

### How Plugins Work

1. **Registration**: Plugins use the `ESPHOME_PLUGIN_REGISTER` macro with GCC's `__attribute__((constructor))` to auto-register during binary load
2. **Initialization**: The core calls each plugin's `init` function during startup
3. **Message handling**: When clients send messages, the core routes them to plugin handlers
4. **Cleanup**: Plugins' `cleanup` functions are called during shutdown

### Plugin Lifecycle

```
Binary Load
    ↓
Plugin Auto-Registration (via GCC constructor)
    ↓
Core Initialization
    ↓
plugin->init() called
    ↓
Main Loop: Message routing
    ↓
    ├─ Client Message → plugin->handle_message()
    ├─ Plugin Response → esphome_plugin_send_message()
    └─ Repeat
    ↓
Shutdown
    ↓
plugin->cleanup() called
```

## Creating Your First Plugin

### Step 1: Create Plugin Directory

```bash
mkdir -p plugins/my_feature
cd plugins/my_feature
```

### Step 2: Create Plugin Source

Create `my_feature_plugin.c`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/include/esphome_plugin.h"

/* Plugin state structure */
typedef struct {
    int my_value;
    bool is_enabled;
} my_plugin_state_t;

/* Initialize plugin */
static int my_plugin_init(esphome_plugin_context_t *ctx) {
    printf("[MyPlugin] Initializing...\n");

    /* Allocate plugin state */
    my_plugin_state_t *state = calloc(1, sizeof(my_plugin_state_t));
    if (!state) {
        return -1;
    }

    state->my_value = 0;
    state->is_enabled = true;

    /* Store in context */
    ctx->plugin_data = state;

    printf("[MyPlugin] Initialized on device: %s\n", ctx->config->device_name);
    return 0;
}

/* Cleanup plugin */
static void my_plugin_cleanup(esphome_plugin_context_t *ctx) {
    printf("[MyPlugin] Cleaning up...\n");
    if (ctx->plugin_data) {
        free(ctx->plugin_data);
        ctx->plugin_data = NULL;
    }
}

/* Handle incoming messages */
static int my_plugin_handle_message(esphome_plugin_context_t *ctx,
                                     uint32_t msg_type,
                                     const uint8_t *data,
                                     size_t len) {
    my_plugin_state_t *state = (my_plugin_state_t *)ctx->plugin_data;

    switch (msg_type) {
    case 1001: /* Custom message type */
        printf("[MyPlugin] Received message type 1001\n");

        /* Process the message */
        if (len >= 4) {
            state->my_value = *(int*)data;
        }

        /* Send response */
        uint8_t response[4];
        *(int*)response = state->my_value;
        esphome_plugin_send_message(ctx, 1002, response, sizeof(response));

        return 0; /* Handled */

    default:
        return -1; /* Not handled */
    }
}

/* Register the plugin */
ESPHOME_PLUGIN_REGISTER(my_plugin, "MyFeature", "1.0.0") = {
    .init = my_plugin_init,
    .cleanup = my_plugin_cleanup,
    .handle_message = my_plugin_handle_message,
};
```

### Step 3: Build

```bash
cd ../..  # Back to project root
meson setup build
meson compile -C build
```

The plugin is automatically detected and compiled!

## Protobuf Message Handling

### ESPHome Native API Protocol

All ESPHome Native API messages use Google Protocol Buffers (protobuf) for serialization. The official protocol definition is maintained in the ESPHome repository:

**Reference**: [esphome/components/api/api.proto](https://github.com/esphome/esphome/blob/dev/esphome/components/api/api.proto)

This file defines all message types and their structure. Plugin developers should refer to this for the authoritative message format.

### Protobuf Encoding/Decoding API

The `esphome_proto.h` header provides functions for encoding and decoding protobuf messages:

#### Buffer Management

```c
/* Initialize buffer for writing */
void pb_buffer_init_write(pb_buffer_t *buf, uint8_t *data, size_t size);

/* Initialize buffer for reading */
void pb_buffer_init_read(pb_buffer_t *buf, const uint8_t *data, size_t size);
```

#### Encoding Functions

```c
/* Encode a varint (variable-length integer) */
bool pb_encode_varint(pb_buffer_t *buf, uint64_t value);

/* Encode a string field */
bool pb_encode_string(pb_buffer_t *buf, uint32_t field_num, const char *str);

/* Encode a boolean field */
bool pb_encode_bool(pb_buffer_t *buf, uint32_t field_num, bool value);

/* Encode a uint32 field */
bool pb_encode_uint32(pb_buffer_t *buf, uint32_t field_num, uint32_t value);

/* Encode a uint64 field (varint encoding) */
bool pb_encode_uint64(pb_buffer_t *buf, uint32_t field_num, uint64_t value);

/* Encode a sint32 field (zigzag encoding for signed integers) */
bool pb_encode_sint32(pb_buffer_t *buf, uint32_t field_num, int32_t value);

/* Encode a fixed64 field (8 bytes, little-endian) */
bool pb_encode_fixed64(pb_buffer_t *buf, uint32_t field_num, uint64_t value);

/* Encode a bytes field (arbitrary binary data) */
bool pb_encode_bytes(pb_buffer_t *buf, uint32_t field_num,
                     const uint8_t *data, size_t len);
```

#### Decoding Functions

```c
/* Decode a varint */
bool pb_decode_varint(pb_buffer_t *buf, uint64_t *value);

/* Decode a string field */
bool pb_decode_string(pb_buffer_t *buf, char *str, size_t max_len);

/* Decode a uint32 field */
bool pb_decode_uint32(pb_buffer_t *buf, uint32_t *value);

/* Skip a field (useful for unknown/unused fields) */
bool pb_skip_field(pb_buffer_t *buf, uint8_t wire_type);
```

### Protobuf Wire Types

```c
#define PB_WIRE_TYPE_VARINT    0  /* int32, int64, uint32, uint64, sint32, sint64, bool, enum */
#define PB_WIRE_TYPE_64BIT     1  /* fixed64, sfixed64, double */
#define PB_WIRE_TYPE_LENGTH    2  /* string, bytes, embedded messages, packed repeated fields */
#define PB_WIRE_TYPE_32BIT     5  /* fixed32, sfixed32, float */
```

### Field Tag Encoding

Protobuf field tags combine the field number and wire type:

```c
#define PB_FIELD_TAG(field_num, wire_type) (((field_num) << 3) | (wire_type))
```

**Example**: Field 5 with wire type VARINT → `(5 << 3) | 0 = 40 = 0x28`

### Example: Encoding a Message

Here's how to encode a BinarySensorStateResponse (message type 21):

```c
#include "../../src/include/esphome_proto.h"

/* Binary Sensor State Response structure (from api.proto)
 * message BinarySensorStateResponse {
 *   uint32 key = 1;        // Entity key
 *   bool state = 2;        // Sensor state (true/false)
 *   bool missing_state = 3; // Whether state is valid
 * }
 */

int send_binary_sensor_state(esphome_plugin_context_t *ctx,
                              uint32_t key, bool state) {
    uint8_t buf[64];
    pb_buffer_t pb;

    /* Initialize buffer for writing */
    pb_buffer_init_write(&pb, buf, sizeof(buf));

    /* Encode field 1: key (uint32) */
    pb_encode_uint32(&pb, 1, key);

    /* Encode field 2: state (bool) */
    pb_encode_bool(&pb, 2, state);

    /* Encode field 3: missing_state = false */
    pb_encode_bool(&pb, 3, false);

    /* Send the message */
    if (!pb.error) {
        return esphome_plugin_send_message(ctx,
                                          ESPHOME_MSG_BINARY_SENSOR_STATE_RESPONSE,
                                          buf, pb.pos);
    }

    return -1;
}
```

### Example: Decoding a Message

Here's how to decode a SwitchCommandRequest (message type 33):

```c
/* Switch Command Request structure (from api.proto)
 * message SwitchCommandRequest {
 *   uint32 key = 1;    // Entity key
 *   bool state = 2;    // Desired state (true=on, false=off)
 * }
 */

static int handle_switch_command(esphome_plugin_context_t *ctx,
                                  const uint8_t *data, size_t len) {
    pb_buffer_t pb;
    uint32_t key = 0;
    bool state = false;

    /* Initialize buffer for reading */
    pb_buffer_init_read(&pb, data, len);

    /* Decode fields */
    while (pb.pos < pb.size && !pb.error) {
        uint64_t tag;
        if (!pb_decode_varint(&pb, &tag)) {
            break;
        }

        uint32_t field_num = tag >> 3;
        uint8_t wire_type = tag & 0x07;

        switch (field_num) {
        case 1: /* key */
            if (wire_type == PB_WIRE_TYPE_VARINT) {
                uint64_t val;
                pb_decode_varint(&pb, &val);
                key = (uint32_t)val;
            }
            break;

        case 2: /* state */
            if (wire_type == PB_WIRE_TYPE_VARINT) {
                uint64_t val;
                pb_decode_varint(&pb, &val);
                state = (bool)val;
            }
            break;

        default:
            /* Unknown field - skip it */
            pb_skip_field(&pb, wire_type);
            break;
        }
    }

    if (!pb.error) {
        printf("[Plugin] Switch %u command: %s\n", key, state ? "ON" : "OFF");
        /* Apply the command to your hardware */
        return 0;
    }

    return -1;
}
```

### Common Message Patterns

#### List Entities Pattern

When handling `LIST_ENTITIES_REQUEST`, send entity info messages followed by `LIST_ENTITIES_DONE_RESPONSE`:

```c
/* From api.proto:
 * message ListEntitiesRequest {}
 * message ListEntitiesDoneResponse {}
 * message ListEntitiesBinarySensorResponse {
 *   string object_id = 1;
 *   uint32 key = 2;
 *   string name = 3;
 *   string unique_id = 4;
 *   string device_class = 5;
 *   bool is_status_binary_sensor = 6;
 *   bool disabled_by_default = 7;
 *   string icon = 8;
 *   EntityCategory entity_category = 9;
 * }
 */

static int my_plugin_list_entities(esphome_plugin_context_t *ctx, int client_id) {
    uint8_t buf[256];
    pb_buffer_t pb;

    /* Encode BinarySensorInfo */
    pb_buffer_init_write(&pb, buf, sizeof(buf));

    pb_encode_string(&pb, 1, "motion");           // object_id
    pb_encode_uint32(&pb, 2, 1);                  // key
    pb_encode_string(&pb, 3, "Motion Sensor");    // name
    pb_encode_string(&pb, 4, "motion_001");       // unique_id
    pb_encode_string(&pb, 5, "motion");           // device_class
    pb_encode_bool(&pb, 6, false);                // is_status_binary_sensor
    pb_encode_bool(&pb, 7, false);                // disabled_by_default
    pb_encode_string(&pb, 8, "mdi:motion-sensor"); // icon
    pb_encode_uint32(&pb, 9, 0);                  // entity_category (NONE)

    if (!pb.error) {
        esphome_plugin_send_message_to_client(ctx, client_id,
                                              12, /* ListEntitiesBinarySensorResponse */
                                              buf, pb.pos);
    }

    return 0;
}
```

#### Subscribe States Pattern

After client subscribes with `SUBSCRIBE_STATES_REQUEST`, send initial states:

```c
/* Send initial state after subscription */
static void send_initial_states(esphome_plugin_context_t *ctx) {
    /* Send state for each entity you expose */
    send_binary_sensor_state(ctx, 1, true);  // motion sensor is triggered
    send_switch_state(ctx, 2, false);        // switch is off
    /* etc. */
}
```

### Pre-encoded Message Helpers

The `esphome_proto.h` header provides helper functions for common messages:

```c
/* Encode a complete DeviceInfoResponse */
size_t esphome_encode_device_info_response(uint8_t *buf, size_t size,
                                           const esphome_device_info_response_t *msg);

/* Encode BLE advertisements (for bluetooth_proxy plugin) */
size_t esphome_encode_ble_advertisements(uint8_t *buf, size_t size,
                                         const esphome_ble_advertisements_response_t *msg);

/* Decode subscription request */
bool esphome_decode_subscribe_ble_advertisements(const uint8_t *buf, size_t size,
                                                 esphome_subscribe_ble_advertisements_t *msg);
```

### Message Framing

Messages are framed with a length prefix and type:

```c
/* Frame a message (adds length + type header) */
size_t esphome_frame_message(uint8_t *out_buf, size_t out_size,
                             uint16_t msg_type,
                             const uint8_t *payload, size_t payload_len);

/* Decode message header (returns payload offset) */
size_t esphome_decode_frame_header(const uint8_t *buf, size_t size,
                                   uint32_t *msg_len, uint16_t *msg_type);
```

**Note**: The plugin API (`esphome_plugin_send_message`) handles framing automatically - you only need to provide the protobuf-encoded payload.

### Best Practices for Protobuf

1. **Always check the official api.proto** - Message formats may change between ESPHome versions
2. **Handle unknown fields gracefully** - Use `pb_skip_field()` for forward compatibility
3. **Validate all decoded data** - Don't trust sizes/ranges from network input
4. **Use appropriate wire types** - Match the types defined in api.proto
5. **Test with real Home Assistant** - Verify compatibility with actual ESPHome integration

### Debugging Protobuf Messages

To debug protobuf encoding/decoding:

```c
/* Dump raw bytes */
void dump_hex(const uint8_t *data, size_t len) {
    printf("Protobuf dump (%zu bytes):\n", len);
    for (size_t i = 0; i < len; i++) {
        if (i % 16 == 0) printf("%04zx: ", i);
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (len % 16 != 0) printf("\n");
}

/* Use in your message handlers */
dump_hex(data, len);  // Dump received message
dump_hex(buf, pb.pos); // Dump encoded message before sending
```

## Plugin API Reference

### Core Types

#### `esphome_plugin_context_t`

Context passed to all plugin callbacks:

```c
struct esphome_plugin_context {
    esphome_api_server_t *server;           /* API server instance */
    const esphome_device_config_t *config;  /* Device configuration */
    void *plugin_data;                       /* Plugin-specific data */
};
```

#### `esphome_plugin_t`

Plugin descriptor:

```c
struct esphome_plugin {
    const char *name;                        /* Plugin name */
    const char *version;                     /* Plugin version */
    esphome_plugin_init_fn init;             /* Init callback */
    esphome_plugin_cleanup_fn cleanup;       /* Cleanup callback */
    esphome_plugin_msg_handler_fn handle_message; /* Message handler */
};
```

### Callback Functions

#### `esphome_plugin_init_fn`

```c
typedef int (*esphome_plugin_init_fn)(esphome_plugin_context_t *ctx);
```

Called during startup. Return 0 on success, -1 on error.

#### `esphome_plugin_cleanup_fn`

```c
typedef void (*esphome_plugin_cleanup_fn)(esphome_plugin_context_t *ctx);
```

Called during shutdown. Free all resources.

#### `esphome_plugin_msg_handler_fn`

```c
typedef int (*esphome_plugin_msg_handler_fn)(esphome_plugin_context_t *ctx,
                                               uint32_t msg_type,
                                               const uint8_t *data,
                                               size_t len);
```

Called for each incoming message. Return 0 if handled, -1 if not.

### Plugin Functions

#### `esphome_plugin_send_message`

```c
int esphome_plugin_send_message(esphome_plugin_context_t *ctx,
                                 uint32_t msg_type,
                                 const uint8_t *data,
                                 size_t len);
```

Send a message to all connected clients.

#### `esphome_plugin_send_message_to_client`

```c
int esphome_plugin_send_message_to_client(esphome_plugin_context_t *ctx,
                                           int client_id,
                                           uint32_t msg_type,
                                           const uint8_t *data,
                                           size_t len);
```

Send a message to a specific client.

#### `esphome_plugin_log`

```c
void esphome_plugin_log(esphome_plugin_context_t *ctx,
                        int level,
                        const char *format, ...);
```

Log a message. Levels: 0=error, 1=warning, 2=info, 3=debug.

### Registration Macro

#### `ESPHOME_PLUGIN_REGISTER`

```c
ESPHOME_PLUGIN_REGISTER(var_name, "PluginName", "1.0.0") = {
    .init = init_fn,
    .cleanup = cleanup_fn,
    .handle_message = handler_fn,
};
```

Registers a plugin with automatic initialization.

### Message Types

#### ESPHome Messages a plugin can handle or send

```c
#define ESPHOME_MSG_BINARY_SENSOR_STATE_RESPONSE                 21
#define ESPHOME_MSG_COVER_STATE_RESPONSE                         22
#define ESPHOME_MSG_FAN_STATE_RESPONSE                           23
#define ESPHOME_MSG_LIGHT_STATE_RESPONSE                         24
#define ESPHOME_MSG_SENSOR_STATE_RESPONSE                        25
#define ESPHOME_MSG_SWITCH_STATE_RESPONSE                        26
#define ESPHOME_MSG_TEXT_SENSOR_STATE_RESPONSE                   27
#define ESPHOME_MSG_SUBSCRIBE_HOMEASSISTANT_SERVICES_REQUEST     34
#define ESPHOME_MSG_HOMEASSISTANT_SERVICE_RESPONSE               35
#define ESPHOME_MSG_SUBSCRIBE_HOMEASSISTANT_STATES_REQUEST       38
#define ESPHOME_MSG_SUBSCRIBE_HOMEASSISTANT_STATE_RESPONSE       39

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
```

## Building with Plugins

### Local Build

Plugins in `plugins/` are automatically detected:

```bash
meson setup build
meson compile -C build
```

### Disable Plugins

```bash
meson setup build -Denable_plugins=false
meson compile -C build
```

### Cross-Compilation for MIPS

```bash
./scripts/build-ingenic-t31.sh
```

Plugins are included in cross-compilation automatically.

## Deployment

### Option 1: Include in Source Tree

Add your plugin to the `plugins/` directory and commit to your fork:

```
esphome-linux-proxy/
├── plugins/
│   ├── media_player/
│   │   ├── media_player_plugin.c
│   │   └── README.md
│   └── voice_assistant/
│       ├── voice_assistant_plugin.c
│       ├── audio_capture.c
│       └── README.md
```

### Option 2: Separate Repository

Maintain plugins in a separate repository and use buildroot hooks to copy them during build.

## Examples

### MediaPlayer Plugin

See `plugins/media_player/media_player_plugin.c` for a complete example.

Key features demonstrated:
- State management
- Command handling
- State reporting
- Message routing

### VoiceAssistant Plugin (Conceptual)

```c
#include "../../src/include/esphome_plugin.h"
#include <alsa/asoundlib.h>

typedef struct {
    snd_pcm_t *capture_handle;
    snd_pcm_t *playback_handle;
    pthread_t audio_thread;
    bool is_listening;
} voice_assistant_state_t;

static int voice_assistant_init(esphome_plugin_context_t *ctx) {
    voice_assistant_state_t *state = calloc(1, sizeof(voice_assistant_state_t));

    /* Initialize ALSA */
    snd_pcm_open(&state->capture_handle, "default", SND_PCM_STREAM_CAPTURE, 0);
    snd_pcm_open(&state->playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);

    /* Configure PCM parameters */
    /* ... */

    ctx->plugin_data = state;
    return 0;
}

static int voice_assistant_handle_message(esphome_plugin_context_t *ctx,
                                           uint32_t msg_type,
                                           const uint8_t *data,
                                           size_t len) {
    voice_assistant_state_t *state = ctx->plugin_data;

    switch (msg_type) {
    case ESPHOME_MSG_VOICE_ASSISTANT_REQUEST:
        /* Start listening */
        state->is_listening = true;
        /* Capture audio and send to Home Assistant */
        break;
    }

    return 0;
}

ESPHOME_PLUGIN_REGISTER(voice_assistant, "VoiceAssistant", "1.0.0") = {
    .init = voice_assistant_init,
    .cleanup = voice_assistant_cleanup,
    .handle_message = voice_assistant_handle_message,
};
```

### Climate Control Plugin (Conceptual)

```c
#include "../../src/include/esphome_plugin.h"

#define ESPHOME_MSG_CLIMATE_COMMAND  1005
#define ESPHOME_MSG_CLIMATE_STATE    1006

typedef struct {
    float target_temperature;
    float current_temperature;
    uint8_t mode; /* 0=off, 1=heat, 2=cool, 3=auto */
} climate_state_t;

static int climate_init(esphome_plugin_context_t *ctx) {
    climate_state_t *state = calloc(1, sizeof(climate_state_t));
    state->target_temperature = 22.0;
    state->current_temperature = 20.0;
    state->mode = 0;

    ctx->plugin_data = state;
    return 0;
}

static int climate_handle_message(esphome_plugin_context_t *ctx,
                                   uint32_t msg_type,
                                   const uint8_t *data,
                                   size_t len) {
    climate_state_t *state = ctx->plugin_data;

    if (msg_type == ESPHOME_MSG_CLIMATE_COMMAND) {
        /* Parse command */
        state->target_temperature = *(float*)(data + 0);
        state->mode = data[4];

        /* Apply to HVAC system via GPIO, I2C, etc. */

        /* Send state update */
        uint8_t response[9];
        *(float*)(response + 0) = state->target_temperature;
        *(float*)(response + 4) = state->current_temperature;
        response[8] = state->mode;

        esphome_plugin_send_message(ctx, ESPHOME_MSG_CLIMATE_STATE,
                                     response, sizeof(response));
        return 0;
    }

    return -1;
}

ESPHOME_PLUGIN_REGISTER(climate, "Climate", "1.0.0") = {
    .init = climate_init,
    .cleanup = climate_cleanup,
    .handle_message = climate_handle_message,
};
```

## Best Practices

### Memory Management

- Always free allocated resources in `cleanup`
- Check return values from `malloc`/`calloc`
- Avoid memory leaks - use valgrind for testing

### Threading

- Plugin callbacks are called from the main thread
- If you need background processing, create your own threads
- Use mutexes to protect shared state
- Clean up threads in `cleanup`

### Error Handling

- Return -1 from `init` on fatal errors
- Log errors using `esphome_plugin_log`
- Handle partial initialization gracefully

### Message Protocol

- Define clear message types in the 1000-9999 range
- Document your message format
- Use proper serialization (consider protocol buffers)
- Validate all incoming data

### Dependencies

If your plugin needs external libraries:

1. Create `plugins/my_feature/meson.build`:
   ```meson
   # Add dependency
   alsa_dep = dependency('alsa', required: true)

   # Add to plugin sources with dependencies
   plugin_sources += files('my_feature_plugin.c')
   deps += alsa_dep
   ```

2. Document dependencies in plugin README.md

### Testing

Test your plugin:

1. **Unit tests**: Test individual functions
2. **Integration tests**: Test with actual Home Assistant
3. **Cross-platform**: Test on x86_64, ARM64, and MIPS if applicable
4. **Memory**: Run valgrind to detect leaks

## Troubleshooting

### Plugin Not Loading

- Check that `ESPHOME_PLUGIN_REGISTER` is used correctly
- Verify plugin is in `plugins/` directory
- Ensure `enable_plugins=true` (default)
- Check build output for compilation errors

### Messages Not Handled

- Verify message type is in plugin range (1000-9999)
- Check `handle_message` returns 0 for handled messages
- Add debug logging to trace message flow

### Crashes

- Check for NULL pointers in `plugin_data`
- Validate all array accesses
- Use `gdb` or `valgrind` to debug

## Contributing

To contribute a plugin to this repository:

1. Create plugin in `plugins/your_feature/`
2. Add comprehensive README.md
3. Document message protocol
4. Include example usage
5. Test on multiple architectures
6. Submit pull request

## Resources

### ESPHome Protocol Documentation

- **[ESPHome API Protocol Buffer Definition (api.proto)](https://github.com/esphome/esphome/blob/dev/esphome/components/api/api.proto)** - The authoritative source for all message types and structures
- [ESPHome Native API Overview](https://esphome.io/components/api.html) - High-level documentation
- [Protocol Buffer Encoding Guide](https://protobuf.dev/programming-guides/encoding/) - Understanding protobuf wire format

### Integration Documentation

- [Home Assistant ESPHome Integration](https://www.home-assistant.io/integrations/esphome/)
- [ESPHome Component Index](https://esphome.io/index.html)

### Build System

- [Buildroot Manual](https://buildroot.org/downloads/manual/manual.html)
- [Meson Build System](https://mesonbuild.com/Manual.html)

### Example Code

- [Bluetooth Proxy Plugin](../plugins/bluetooth_proxy/) - Complete reference implementation
- [Plugin Architecture Overview](../PLUGIN_ARCHITECTURE.md)

## License

Plugins are subject to the same MIT license as the main project unless otherwise specified.
