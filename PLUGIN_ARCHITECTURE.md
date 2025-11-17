# Plugin Architecture Overview

This document provides a high-level overview of the plugin system for ESPHome Linux.

## Quick Start

### For Plugin Users

To build with all plugins (including bluetooth_proxy):

```bash
meson setup build
meson compile -C build
```

To build without any plugins (core functionality only):

```bash
meson setup build -Denable_plugins=false
meson compile -C build
```

Note: Disabling plugins will remove BLE scanning functionality (bluetooth_proxy plugin).

To disable only the Bluetooth Proxy plugin:

```bash
meson setup build -Denable_bluetooth_proxy=false
meson compile -C build
```

### For Plugin Developers

Create a new plugin in 3 steps:

1. **Create plugin directory**:
   ```bash
   mkdir -p plugins/my_feature
   ```

2. **Write plugin code** (`plugins/my_feature/my_plugin.c`):
   ```c
   #include "../../src/include/esphome_plugin.h"

   static int my_plugin_init(esphome_plugin_context_t *ctx) {
       // Initialize your plugin
       return 0;
   }

   static void my_plugin_cleanup(esphome_plugin_context_t *ctx) {
       // Cleanup resources
   }

   static int my_plugin_handle_message(esphome_plugin_context_t *ctx,
                                        uint32_t msg_type,
                                        const uint8_t *data,
                                        size_t len) {
       // Handle ESPHome messages
       return 0; // 0 = handled, -1 = not handled
   }

   ESPHOME_PLUGIN_REGISTER(my_plugin, "MyFeature", "1.0.0") = {
       .init = my_plugin_init,
       .cleanup = my_plugin_cleanup,
       .handle_message = my_plugin_handle_message,
       .configure_device_info = NULL,  // Optional: to configure device capabilities
       .list_entities = NULL,          // Optional: to register entities
   };
   ```

3. **Build**:
   ```bash
   meson setup build
   meson compile -C build
   ```

That's it! Your plugin is automatically detected and compiled.

## Architecture

### Plugin System Components

```
┌─────────────────────────────────────────────────────────┐
│                    esphome-linux                         │
│  ┌───────────────────────────────────────────────────┐  │
│  │             Core (main, esphome_api)              │  │
│  └───────────────────────────────────────────────────┘  │
│                            │                             │
│                            ▼                             │
│  ┌───────────────────────────────────────────────────┐  │
│  │          Plugin Manager (auto-registration)       │  │
│  └───────────────────────────────────────────────────┘  │
│           │                 │                 │          │
│           ▼                 ▼                 ▼          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐     │
│  │Bluetooth    │  │MediaPlayer  │  │   Custom    │     │
│  │Proxy        │  │   Plugin    │  │   Plugins   │     │
│  │(included)   │  │  (future)   │  │             │     │
│  └─────────────┘  └─────────────┘  └─────────────┘     │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
            ┌────────────────────────────────┐
            │      ESPHome Native API        │
            │     (Home Assistant, etc.)     │
            └────────────────────────────────┘
```

### Message Flow

```
Home Assistant
      │
      ▼ (ESPHome Native API Message)
ESPHome API Server
      │
      ▼ (Route message to plugins)
Plugin Manager
      │
      ├──▶ Plugin 1: handle_message() → Returns -1 (not handled)
      ├──▶ Plugin 2: handle_message() → Returns 0 (handled!)
      │                    │
      │                    ▼
      │         Process message, update state
      │                    │
      │                    ▼
      │         esphome_plugin_send_message()
      │                    │
      ▼                    ▼
ESPHome API Server ◀──────┘
      │
      ▼
Home Assistant (receives state update)
```

## Files Added/Modified

### New Files

1. **`src/include/esphome_plugin.h`** - Plugin API header
   - Plugin registration macros
   - Callback type definitions
   - Message type constants
   - API function declarations

2. **`meson_options.txt`** - Build options
   - `enable_plugins` option (default: true)

3. **`plugins/media_player/media_player_plugin.c`** - Example plugin
   - Demonstrates plugin structure
   - Shows message handling
   - Template for new plugins

4. **`plugins/media_player/README.md`** - Plugin-specific docs

5. **`docs/PLUGIN_DEVELOPMENT.md`** - Comprehensive plugin guide
   - API reference
   - Examples
   - Best practices
   - Troubleshooting

6. **`PLUGIN_ARCHITECTURE.md`** - This file

### Modified Files

1. **`meson.build`** - Added plugin detection and compilation
   - Scans `plugins/` directory
   - Auto-includes .c files
   - Supports plugin-specific meson.build files

2. **`BUILDROOT_INTEGRATION.md`** - Added plugin deployment options
   - Fork and include plugins
   - Buildroot overlay method
   - Separate plugin packages

## Plugin API

### Core Functions

```c
/* Send message to all clients */
int esphome_plugin_send_message(esphome_plugin_context_t *ctx,
                                 uint32_t msg_type,
                                 const uint8_t *data,
                                 size_t len);

/* Send message to specific client */
int esphome_plugin_send_message_to_client(esphome_plugin_context_t *ctx,
                                           int client_id,
                                           uint32_t msg_type,
                                           const uint8_t *data,
                                           size_t len);

/* Log message */
void esphome_plugin_log(esphome_plugin_context_t *ctx,
                        int level,
                        const char *format, ...);
```

### Plugin Callbacks

```c
/* Initialize plugin (return 0 on success, -1 on error) */
typedef int (*esphome_plugin_init_fn)(esphome_plugin_context_t *ctx);

/* Cleanup plugin resources */
typedef void (*esphome_plugin_cleanup_fn)(esphome_plugin_context_t *ctx);

/* Handle incoming messages (return 0 if handled, -1 if not) */
typedef int (*esphome_plugin_msg_handler_fn)(esphome_plugin_context_t *ctx,
                                               uint32_t msg_type,
                                               const uint8_t *data,
                                               size_t len);

/* Configure device info response (optional, return 0 on success) */
typedef int (*esphome_plugin_configure_device_info_fn)(
    esphome_plugin_context_t *ctx,
    esphome_device_info_response_t *device_info);

/* List entities exposed by plugin (optional, return 0 on success) */
typedef int (*esphome_plugin_list_entities_fn)(
    esphome_plugin_context_t *ctx,
    int client_id);
```

#### Optional Callbacks

**`configure_device_info`**: Called when building the device info response during handshake.
Plugins use this to advertise their capabilities (e.g., Bluetooth proxy features, Voice Assistant support).

**`list_entities`**: Called when a client requests the entity list. Plugins can send entity
info messages for sensors, switches, or other entities they expose.

## Use Cases

### Bluetooth Proxy Plugin (Included)

Core functionality implemented as a plugin:
- BLE advertisement scanning via BlueZ
- Device caching and batching
- Periodic reporting to Home Assistant
- Subscribe/unsubscribe handling

**Status**: Production-ready, included by default

### Potential Additional Plugins

**MediaPlayer**: Audio playback functionality
- Play/pause/stop controls, volume, ALSA/PulseAudio integration

**VoiceAssistant**: Voice control
- Microphone capture, wake word detection, TTS playback

**Climate**: HVAC control
- Temperature/mode control, GPIO/I2C hardware integration

**Sensors**: Hardware sensor integration
- I2C sensors, GPIO motion detection, custom UART protocols

## Integration Methods

### Method 1: Source Tree (Simple)

Add plugins directly to the repository:

```
esphome-linux/
└── plugins/
    ├── bluetooth_proxy/    # Included by default
    ├── voice_assistant/    # Your addition
    └── my_custom_plugin/   # Your addition
```

Build normally - plugins are auto-detected.

### Method 2: Buildroot Overlay (Advanced)

Use buildroot's overlay system:

```
thingino-firmware/
└── board/thingino/overlay/esphome-linux-plugins/
    └── custom_plugin/
        └── plugin.c
```

Buildroot copies plugins before building.

### Method 3: Separate Package (Modular)

Create a separate buildroot package:

```
package/esphome-linux-mediaplayer/
├── Config.in
└── esphome-linux-mediaplayer.mk
```

Package installs plugin into main project during build.

## Benefits

- **Extensibility**: Add features without modifying core code
- **Modularity**: Plugins can be enabled/disabled independently
- **Maintainability**: Plugin bugs don't affect core functionality
- **Community**: Easy for others to contribute plugins
- **Flexibility**: Multiple deployment options for different use cases
- **Performance**: Only compile what you need

## Next Steps

### For Users

1. Review existing plugins in `plugins/` directory
2. Enable/disable plugins as needed
3. Build and deploy to your device

### For Developers

1. Read [docs/PLUGIN_DEVELOPMENT.md](docs/PLUGIN_DEVELOPMENT.md)
2. Study the MediaPlayer example plugin
3. Create your plugin in `plugins/your_feature/`
4. Test and submit a pull request

### For Buildroot Integration

1. Review [BUILDROOT_INTEGRATION.md](BUILDROOT_INTEGRATION.md)
2. Choose integration method
3. Set up buildroot package
4. Test in firmware build

## License

The plugin system and all included example plugins are licensed under MIT, same as the main project.

## Support

- **Issues**: Open an issue in the repository
- **Documentation**: [docs/PLUGIN_DEVELOPMENT.md](docs/PLUGIN_DEVELOPMENT.md)
- **Examples**: [plugins/bluetooth_proxy/](plugins/bluetooth_proxy/)

## Contributing

Contributions welcome! To add a plugin:

1. Fork the repository
2. Create your plugin in `plugins/your_feature/`
3. Add documentation
4. Test on target hardware
5. Submit pull request

For plugin API enhancements, please open an issue first to discuss.
