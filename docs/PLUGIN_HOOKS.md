# Plugin Hooks Reference

This document describes the plugin hook system that allows plugins to integrate with the ESPHome Native API handshake and entity system.

## Overview

The ESPHome plugin system provides two optional hooks in addition to the core `init`, `cleanup`, and `handle_message` callbacks:

1. **`configure_device_info`** - Configure device capabilities during handshake
2. **`list_entities`** - Register entities exposed by the plugin

## Device Info Configuration Hook

### Purpose

The `configure_device_info` hook allows plugins to modify the Device Info Response that is sent to clients during the initial handshake. This is how plugins advertise their capabilities and features to Home Assistant.

### Signature

```c
typedef int (*esphome_plugin_configure_device_info_fn)(
    esphome_plugin_context_t *ctx,
    esphome_device_info_response_t *device_info);
```

### When Called

This hook is called when a client sends a `DEVICE_INFO_REQUEST` message (type 9) during the connection handshake, after the core fields (name, MAC, version, etc.) have been populated.

### Parameters

- **`ctx`**: Plugin context with access to server and device config
- **`device_info`**: Pointer to the device info response structure to modify

### Return Value

- **0**: Success
- **-1**: Error (logged but doesn't fail the handshake)

### Device Info Fields

Plugins can modify these fields in the `device_info` structure:

```c
typedef struct {
    /* Core device information */
    bool uses_password;                      /* Field 1 */
    char name[128];                          /* Field 2 */
    char mac_address[24];                    /* Field 3 */
    char esphome_version[32];                /* Field 4 */
    char compilation_time[64];               /* Field 5 */
    char model[128];                         /* Field 6 */
    bool has_deep_sleep;                     /* Field 7 */
    char project_name[128];                  /* Field 8 - Optional ESPHome project name */
    char project_version[128];               /* Field 9 - Optional project version */
    uint32_t webserver_port;                 /* Field 10 - Optional webserver port */
    char manufacturer[128];                  /* Field 12 */
    char friendly_name[128];                 /* Field 13 */
    char suggested_area[64];                 /* Field 16 */

    /* Feature flags - plugins modify these to advertise capabilities: */
    uint32_t bluetooth_proxy_feature_flags;  /* Field 15 - BLE proxy capabilities */
    uint32_t voice_assistant_feature_flags;  /* Field 17 - Voice assistant capabilities */
    char bluetooth_mac_address[24];          /* Field 18 - Bluetooth MAC address */
    bool api_encryption_supported;           /* Field 19 - API encryption support */

    /* Z-Wave proxy fields */
    uint32_t zwave_proxy_feature_flags;      /* Field 23 - Z-Wave proxy capabilities */
    uint32_t zwave_home_id;                  /* Field 24 - Z-Wave network home ID */

    /* Note: Fields 20-22 are complex nested messages not yet implemented:
     *   20: devices (repeated DeviceInfo) - for multi-device proxies
     *   21: areas (repeated AreaInfo) - area definitions
     *   22: area (AreaInfo) - device's assigned area
     * See api.proto for these advanced features.
     */
} esphome_device_info_response_t;
```

### Example: Bluetooth Proxy Plugin

```c
static int bluetooth_proxy_configure_device_info(
    esphome_plugin_context_t *ctx,
    esphome_device_info_response_t *device_info)
{
    /* Advertise Bluetooth proxy support */
    device_info->bluetooth_proxy_feature_flags =
        BLE_FEATURE_PASSIVE_SCAN | BLE_FEATURE_RAW_ADVERTISEMENTS;

    /* Use the same MAC address for Bluetooth */
    strncpy(device_info->bluetooth_mac_address,
            ctx->config->mac_address,
            sizeof(device_info->bluetooth_mac_address) - 1);

    printf("[bluetooth_proxy] Configured device info: BLE flags = 0x%08x\n",
           device_info->bluetooth_proxy_feature_flags);

    return 0;
}
```

### Bluetooth Proxy Feature Flags

```c
#define BLE_FEATURE_PASSIVE_SCAN       (1 << 0)  /* Passive BLE scanning */
#define BLE_FEATURE_ACTIVE_SCAN        (1 << 1)  /* Active BLE scanning */
#define BLE_FEATURE_REMOTE_CACHE       (1 << 2)  /* Remote caching */
#define BLE_FEATURE_PAIRING            (1 << 3)  /* BLE pairing */
#define BLE_FEATURE_CACHE_CLEARING     (1 << 4)  /* Cache clearing */
#define BLE_FEATURE_RAW_ADVERTISEMENTS (1 << 5)  /* Raw advertisement data */
```

### Voice Assistant Feature Flags

```c
#define VOICE_ASSISTANT_FEATURE_VOICE_ASSISTANT (1 << 0)  /* Basic voice assistant */
#define VOICE_ASSISTANT_FEATURE_SPEAKER        (1 << 1)  /* Speaker support */
#define VOICE_ASSISTANT_FEATURE_API_AUDIO      (1 << 2)  /* API audio streaming */
#define VOICE_ASSISTANT_FEATURE_TIMERS         (1 << 3)  /* Timer support */
```

### Example: Voice Assistant Plugin

```c
static int voice_assistant_configure_device_info(
    esphome_plugin_context_t *ctx,
    esphome_device_info_response_t *device_info)
{
    /* Advertise voice assistant capabilities */
    device_info->voice_assistant_feature_flags =
        VOICE_ASSISTANT_FEATURE_VOICE_ASSISTANT |
        VOICE_ASSISTANT_FEATURE_SPEAKER |
        VOICE_ASSISTANT_FEATURE_API_AUDIO |
        VOICE_ASSISTANT_FEATURE_TIMERS;

    printf("[voice_assistant] Configured device info: VA flags = 0x%08x\n",
           device_info->voice_assistant_feature_flags);

    return 0;
}
```

## Entity List Hook

### Purpose

The `list_entities` hook allows plugins to register entities (sensors, switches, buttons, etc.) that they expose to Home Assistant. This is called during the entity discovery phase.

### Signature

```c
typedef int (*esphome_plugin_list_entities_fn)(
    esphome_plugin_context_t *ctx,
    int client_id);
```

### When Called

This hook is called when a client sends a `LIST_ENTITIES_REQUEST` message (type 11), before the `LIST_ENTITIES_DONE_RESPONSE` is sent.

### Parameters

- **`ctx`**: Plugin context with access to server and device config
- **`client_id`**: ID of the client requesting the entity list

### Return Value

- **0**: Success
- **-1**: Error (logged but doesn't fail the request)

### Usage

Plugins should use `esphome_plugin_send_message_to_client()` to send entity info messages:

```c
static int my_plugin_list_entities(esphome_plugin_context_t *ctx, int client_id)
{
    /* Example: Send a binary sensor entity */
    uint8_t sensor_info[256];
    size_t len = encode_binary_sensor_info(sensor_info, sizeof(sensor_info),
                                           "motion_sensor", "Motion Detected");

    esphome_plugin_send_message_to_client(ctx, client_id,
                                          ESPHOME_MSG_BINARY_SENSOR_INFO,
                                          sensor_info, len);

    /* Example: Send a switch entity */
    uint8_t switch_info[256];
    len = encode_switch_info(switch_info, sizeof(switch_info),
                            "relay1", "Relay 1");

    esphome_plugin_send_message_to_client(ctx, client_id,
                                          ESPHOME_MSG_SWITCH_INFO,
                                          switch_info, len);

    return 0;
}
```

### Entity Message Types

Common entity info message types:

```c
#define ESPHOME_MSG_BINARY_SENSOR_INFO    12
#define ESPHOME_MSG_COVER_INFO            13
#define ESPHOME_MSG_FAN_INFO              14
#define ESPHOME_MSG_LIGHT_INFO            15
#define ESPHOME_MSG_SENSOR_INFO           16
#define ESPHOME_MSG_SWITCH_INFO           17
#define ESPHOME_MSG_TEXT_SENSOR_INFO      18
#define ESPHOME_MSG_CLIMATE_INFO          46
#define ESPHOME_MSG_NUMBER_INFO           47
#define ESPHOME_MSG_SELECT_INFO           48
#define ESPHOME_MSG_BUTTON_INFO           73
/* ... and more */
```

### Example: Media Player Plugin

```c
static int media_player_list_entities(esphome_plugin_context_t *ctx, int client_id)
{
    printf("[media_player] Listing entities for client %d\n", client_id);

    /* Create and send media player entity info */
    uint8_t entity_buf[512];
    size_t len = encode_media_player_info(
        entity_buf, sizeof(entity_buf),
        "media_player",           // object_id
        "Living Room Speaker",    // name
        "media_player_icon",      // icon
        true                      // supports_pause
    );

    if (len > 0) {
        esphome_plugin_send_message_to_client(
            ctx, client_id,
            ESPHOME_MSG_MEDIA_PLAYER_INFO,
            entity_buf, len
        );
    }

    return 0;
}
```

## Registering Hooks

Hooks are registered in the plugin descriptor passed to `ESPHOME_PLUGIN_REGISTER`:

```c
ESPHOME_PLUGIN_REGISTER(my_plugin, "MyPlugin", "1.0.0") = {
    .init = my_plugin_init,
    .cleanup = my_plugin_cleanup,
    .handle_message = my_plugin_handle_message,
    .configure_device_info = my_plugin_configure_device_info,  // Optional
    .list_entities = my_plugin_list_entities,                  // Optional
};
```

Both hooks are **optional** - set to `NULL` if not needed.

## Call Order

During a typical client connection:

1. Client connects and sends `HELLO_REQUEST`
2. Client sends `CONNECT_REQUEST` (authentication)
3. Client sends `DEVICE_INFO_REQUEST`
   - Core fills in basic device info
   - **→ All plugins' `configure_device_info` hooks are called**
   - Response sent to client
4. Client sends `LIST_ENTITIES_REQUEST`
   - **→ All plugins' `list_entities` hooks are called**
   - `LIST_ENTITIES_DONE_RESPONSE` sent
5. Client sends `SUBSCRIBE_STATES_REQUEST`
6. Normal message flow begins

## Best Practices

### For `configure_device_info`

1. **Only modify feature flags relevant to your plugin**
   - Don't overwrite flags set by other plugins
   - Use bitwise OR to combine flags: `|=`

2. **Log what you're configuring**
   - Helps with debugging capability negotiation

3. **Keep it fast**
   - Called during handshake - don't do heavy I/O

### For `list_entities`

1. **Send all entities in one call**
   - Don't defer entity registration

2. **Use consistent entity IDs**
   - IDs should be stable across restarts

3. **Provide meaningful names and icons**
   - Improve UX in Home Assistant

4. **Handle encoding errors gracefully**
   - Check return values from encoding functions

## Use Cases

### When to use `configure_device_info`

- Your plugin adds hardware features (Bluetooth, Voice Assistant, etc.)
- You need to advertise capabilities to Home Assistant
- Your plugin affects what API versions or features are available

### When to use `list_entities`

- Your plugin exposes sensors, switches, or other entities
- You want entities to appear in Home Assistant automatically
- Your plugin manages hardware that has controllable/readable state

### When NOT to use these hooks

- Pure protocol handlers (no entities, no special features)
- Background services that don't interact with Home Assistant UI
- Plugins that only handle messages without exposing entities

## Future Extensions

Planned additions to the device info structure:

- `voice_assistant_feature_flags` - Voice Assistant capabilities
- `media_player_feature_flags` - Media Player capabilities
- `camera_feature_flags` - Camera capabilities
- Custom feature flag fields for community plugins

## See Also

- [PLUGIN_ARCHITECTURE.md](../PLUGIN_ARCHITECTURE.md) - Overview of plugin system
- [esphome_plugin.h](../src/include/esphome_plugin.h) - Plugin API header
- [bluetooth_proxy_plugin.c](../plugins/bluetooth_proxy/bluetooth_proxy_plugin.c) - Reference implementation
