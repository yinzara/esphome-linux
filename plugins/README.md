# Plugins Directory

This directory contains plugin modules that extend the functionality of esphome-linux.

## Included Plugins

### bluetooth_proxy (Core)

Bluetooth LE scanning via BlueZ D-Bus. This is the core functionality - it's implemented as a plugin to demonstrate the architecture.

Features:
- BLE advertisement scanning
- Device caching and batching
- Periodic reporting to Home Assistant
- Handles subscribe/unsubscribe requests

**Status**: Production - fully functional

See [bluetooth_proxy/README.md](bluetooth_proxy/README.md) for details.

## Creating a Plugin

### Minimal Plugin Structure

```
plugins/
└── my_feature/
    ├── README.md           # Plugin documentation
    └── my_feature_plugin.c # Plugin source code
```

### Quick Start

1. Create your plugin directory:
   ```bash
   mkdir -p plugins/my_feature
   ```

2. Create `plugins/my_feature/my_feature_plugin.c`:
   ```c
   #include "../../src/include/esphome_plugin.h"

   static int my_feature_init(esphome_plugin_context_t *ctx) {
       // Initialize
       return 0;
   }

   static void my_feature_cleanup(esphome_plugin_context_t *ctx) {
       // Cleanup
   }

   static int my_feature_handle_message(esphome_plugin_context_t *ctx,
                                         uint32_t msg_type,
                                         const uint8_t *data,
                                         size_t len) {
       // Handle messages
       return -1; // Not handled
   }

   ESPHOME_PLUGIN_REGISTER(my_feature, "MyFeature", "1.0.0") = {
       .init = my_feature_init,
       .cleanup = my_feature_cleanup,
       .handle_message = my_feature_handle_message,
   };
   ```

3. Build:
   ```bash
   cd ../..  # Back to project root
   meson setup build
   meson compile -C build
   ```

Your plugin is automatically detected and compiled!

## Advanced Plugin Structure

For complex plugins with dependencies:

```
plugins/
└── my_feature/
    ├── meson.build         # Build configuration
    ├── README.md           # Documentation
    ├── my_feature_plugin.c # Main plugin code
    ├── helper.c            # Additional source files
    └── include/
        └── my_feature.h    # Private headers
```

Example `meson.build`:

```meson
# Add dependency
alsa_dep = dependency('alsa', required: true)

# Add sources
plugin_sources += files(
  'my_feature_plugin.c',
  'helper.c',
)

# Add to dependencies
deps += alsa_dep
```

## Plugin Ideas

### Core Plugins

- ✅ **bluetooth_proxy** - Bluetooth LE scanning (production)

### Suggested Additional Plugins

- **voice_assistant** - Microphone/speaker for voice control
- **climate** - HVAC control
- **cover** - Blinds/curtain control
- **light** - Smart lighting (beyond BLE)
- **camera** - Video streaming
- **alarm_control_panel** - Security system integration
- **lock** - Smart lock control
- **fan** - Fan speed control
- **sensor** - Hardware sensor integration (I2C, GPIO, etc.)
- **switch** - GPIO switch control
- **binary_sensor** - Motion, door, window sensors
- **number** - Numeric input entities
- **select** - Selection entities

## Plugin Requirements

### Must Have

- Proper initialization and cleanup
- Error handling
- Memory leak prevention
- Documentation (README.md)

### Should Have

- Example usage
- Tested on target hardware
- Thread safety (if using background threads)
- Graceful degradation on errors

### Nice to Have

- Configuration options
- Logging with levels
- Unit tests
- Integration tests with Home Assistant

## Building

### With Plugins (Default)

```bash
meson setup build
meson compile -C build
```

### Without Plugins

```bash
meson setup build -Denable_plugins=false
meson compile -C build
```

### Specific Plugins Only

Remove unwanted plugin directories:

```bash
rm -rf plugins/media_player  # Remove example plugin
meson setup build
meson compile -C build
```

## Documentation

- **Plugin Development Guide**: [../docs/PLUGIN_DEVELOPMENT.md](../docs/PLUGIN_DEVELOPMENT.md)
- **Plugin Architecture**: [../PLUGIN_ARCHITECTURE.md](../PLUGIN_ARCHITECTURE.md)
- **Buildroot Integration**: [../BUILDROOT_INTEGRATION.md](../BUILDROOT_INTEGRATION.md)
- **API Reference**: [../src/include/esphome_plugin.h](../src/include/esphome_plugin.h)

## Contributing Plugins

To contribute a plugin to this repository:

1. **Create** your plugin in this directory
2. **Document** with comprehensive README.md
3. **Test** on actual hardware
4. **Follow** coding style of existing plugins
5. **Submit** pull request

### Plugin Checklist

- [ ] Plugin compiles without warnings
- [ ] Memory leaks checked (valgrind)
- [ ] Tested on target hardware
- [ ] README.md with usage instructions
- [ ] Clear message protocol documented
- [ ] Error handling implemented
- [ ] Resources cleaned up properly
- [ ] Compatible with plugin API version

## License

Plugins inherit the MIT license from the main project unless otherwise specified in the plugin's README.

## Support

- **Issues**: Open an issue in the main repository
- **Questions**: Check [docs/PLUGIN_DEVELOPMENT.md](../docs/PLUGIN_DEVELOPMENT.md)
- **Examples**: Study the media_player example plugin

## Plugin Versioning

Plugins should follow semantic versioning:

- **Major**: Breaking API changes
- **Minor**: New features, backward compatible
- **Patch**: Bug fixes

Example: `"1.2.3"`
