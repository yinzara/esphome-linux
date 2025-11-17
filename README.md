# ESPHome Linux

A lightweight TCP service that implements the [ESPHome Native API](https://esphome.io/components/api/), enabling any Linux device to provide the same functionality as an ESPHome device. Home Assistant will see the device as a native ESPHome device.

This implementation does not currently implement encryption or authentication! Use at your own risk.
Since it only provides BLE scanning functionality currently it was not required but an upgrade to use encryption and authentication would be accepted in a PR.

Note: this project is not affiliated with ESPHome in any way. It is its own standalone implementation.

## Features

- **ESPHome Native API implementation** - Full native C protobuf-based protocol compatibility
- **Plugin architecture** - Easily extend with additional ESPHome API features
- **Bluetooth Proxy plugin included** - BLE scanning via BlueZ D-Bus (included by default)
- **Multi-client support** - Handle multiple Home Assistant connections
- **Cross-platform** - Released for ARM64, AMD64 (x86_64), and Ingenic T31 MIPS; others supported via cross-compilation
- **mDNS integration** - Works with `mdnsd` or `avahi` for automatic discovery
- **Extensible** - Add MediaPlayer, VoiceAssistant, Climate, and other ESPHome features via plugins

## Use Cases

- Embedded IP cameras with Bluetooth (e.g., Thingino firmware)
- Single-board computers (Raspberry Pi, Orange Pi, etc.)
- IoT gateways and edge devices
- Any Linux device wanting to integrate with Home Assistant as an ESPHome device
- Custom hardware needing ESPHome API support beyond traditional ESP devices

## Dependencies

### Build Dependencies

- **Meson** >= 0.55.0 - Build system
- **Ninja** - Build backend
- **pkg-config** - Dependency discovery
- **GCC** or **Clang** - C11 compiler

### Runtime Dependencies

- **D-Bus** >= 1.6 (`libdbus-1`)
- **GLib** >= 2.40 (`libglib-2.0`)
- **pthread** - POSIX threads
- **BlueZ** >= 5.x - Bluetooth stack (`bluetoothd`) - Optional, required only for Bluetooth Proxy plugin
- **mDNS service** - `mdnsd` or `avahi-daemon` recommended for auto-discovery

## Building

### Native Build

```bash
# Install dependencies (Debian/Ubuntu)
sudo apt-get install meson ninja-build pkg-config \
  libdbus-1-dev libglib2.0-dev

# Configure
meson setup build

# Build
meson compile -C build

# Install (optional)
sudo meson install -C build
```

### Cross-Compilation for Ingenic T31 (MIPS)

For Thingino cameras and other Ingenic T31 devices:

```bash
# Single command - uses Docker multi-stage build
./scripts/build.sh --mips

# Or use make
make cross

# Output: ./esphome-linux-mips
```

The script automatically:
- Builds dependencies (dbus, glib, etc.) on first run (~20-30 min)
- **Caches them in Docker layers**
- Subsequent builds only rebuild your app code (~1-2 min)
- Uses multi-stage Dockerfile for optimal caching

**Fast rebuilds:** Change your code and rerun - only the app layer rebuilds!

**For production:** Integrate as a buildroot package (see [BUILDROOT_INTEGRATION.md](BUILDROOT_INTEGRATION.md))

### Plugin System

The project includes a plugin architecture for extensibility:

```bash
# Build with all plugins (default)
meson setup build
meson compile -C build

# Build without plugins (core API server only)
meson setup build -Denable_plugins=false
meson compile -C build
```

See [PLUGIN_ARCHITECTURE.md](PLUGIN_ARCHITECTURE.md) for details on creating plugins.

### Generic Cross-Compilation

For other platforms, create a cross-file or modify `cross/mips-linux.txt`:

```ini
[binaries]
c = 'mipsel-linux-gcc'
ar = 'mipsel-linux-ar'
strip = 'mipsel-linux-strip'
pkg-config = 'pkg-config'

[properties]
sys_root = '/path/to/sysroot'
pkg_config_libdir = '/path/to/sysroot/usr/lib/pkgconfig'

[host_machine]
system = 'linux'
cpu_family = 'mips'
cpu = 'mips32'
endian = 'little'
```

Then build:

```bash
meson setup build --cross-file cross/mips-linux.txt
meson compile -C build
```

### Alternative: Using CROSS_COMPILE

If you prefer traditional `CROSS_COMPILE` variable:

```bash
# Set compiler via environment
export CC=mipsel-linux-gcc
export AR=mipsel-linux-ar
export PKG_CONFIG_PATH=/path/to/sysroot/usr/lib/pkgconfig

meson setup build
meson compile -C build
```

## Running

### Prerequisites

1. **BlueZ daemon must be running:**
   ```bash
   sudo systemctl start bluetooth
   # or
   sudo bluetoothd
   ```

2. **D-Bus system bus must be accessible:**
   ```bash
   # Check D-Bus is running
   dbus-daemon --system
   ```

3. **mDNS for automatic discovery (recommended):**
   ```bash
   # mdnsd (lightweight, recommended for embedded systems)
   # Configuration file:
   #   type _esphomelib._tcp
   #   port 6053
   mdnsd

   # Or use Avahi
   avahi-daemon
   ```

### Start the Service

```bash
./build/esphome-linux
```

The service will:
- Listen on TCP port **6053**
- Load plugins (Bluetooth Proxy plugin by default)
- Connect to BlueZ via D-Bus (if Bluetooth Proxy plugin is enabled)
- Wait for Home Assistant to connect and subscribe to services

### Integration with Home Assistant

1. The service advertises itself via mDNS as `_esphomelib._tcp` (requires mdnsd or avahi)
2. Home Assistant auto-discovers it as an ESPHome device
3. Add it through the ESPHome integration
4. Depending on loaded plugins, it will provide different functionality:
   - **Bluetooth Proxy plugin** - BLE device scanning and forwarding
   - **Future plugins** - MediaPlayer, VoiceAssistant, Climate, etc.

## Configuration

Currently configuration is compile-time:

- **TCP port**: 6053 (standard ESPHome API port, defined in `esphome_api.c`)
- **Device name**: Hostname (auto-detected in `main.c`)
- **Plugin-specific settings**: See individual plugin documentation in `plugins/*/README.md`

## Architecture

```
┌─────────────────────────────────────────┐
│         Home Assistant                  │
│         (ESPHome Client)                │
└────────────────┬────────────────────────┘
                 │ TCP 6053 (ESPHome Native API)
                 │ Protobuf framed messages
                 │
┌────────────────▼────────────────────────┐
│           esphome-linux                 │
│                                         │
│  ┌───────────────────────────────────┐  │
│  │      ESPHome API Server           │  │  - Protocol handshake
│  │      (Core)                       │  │  - Device info
│  └──────────────┬────────────────────┘  │  - Message routing
│                 │                       │
│  ┌──────────────▼────────────────────┐  │
│  │      Plugin Manager               │  │  - Auto-registration
│  │                                   │  │  - Message dispatch
│  └──┬───────────┬──────────────────┬─┘  │
│     │           │                  │    │
│  ┌──▼───────┐ ┌▼────────┐  ┌──────▼──┐ │
│  │Bluetooth │ │MediaPlayer│ │Custom   │ │
│  │Proxy     │ │(future) │  │Plugins  │ │
│  │(BlueZ)   │ └─────────┘  └─────────┘ │
│  └──┬───────┘                          │
│     │                                   │
└─────┼───────────────────────────────────┘
      │ D-Bus (for Bluetooth plugin)
┌─────▼─────────┐
│  bluetoothd   │  - BLE scanning (BlueZ)
│  (Optional)   │  - Advertisement parsing
└───────────────┘
```

## Project Structure

```
esphome-linux/
├── docs/                   # Detailed developer docs
├── src/
│   ├── main.c              # Entry point
│   ├── esphome_api.c       # ESPHome protocol server (core)
│   ├── esphome_proto.c     # Protobuf encoder/decoder
│   └── include/
│       ├── esphome_api.h
│       ├── esphome_proto.h
│       ├── esphome_plugin_internal.h
│       └── esphome_plugin.h  # Plugin API
├── plugins/
│   ├── bluetooth_proxy/     # Bluetooth LE scanning (included)
│   │   ├── ble_scanner.c
│   │   ├── ble_scanner.h
│   │   ├── bluetooth_proxy_plugin.c
│   │   └── README.md
│   └── README.md            # Plugin development guide
├── cross/
│   └── mips-linux.txt       # Cross-compilation config
├── meson.build              # Build configuration
├── PLUGIN_ARCHITECTURE.md   # Architecture overview
├── BUILDROOT_INTEGRATION.md # Buildroot packaging guide
├── README.md
└── LICENSE
```

## Troubleshooting

### No BLE devices appearing

1. Check BlueZ is scanning:
   ```bash
   bluetoothctl
   > scan on
   ```

2. Check D-Bus permissions:
   ```bash
   dbus-send --system --print-reply \
     --dest=org.bluez \
     /org/bluez/hci0 \
     org.freedesktop.DBus.Properties.Get \
     string:org.bluez.Adapter1 string:Discovering
   ```

3. Check service logs for errors

### Home Assistant not discovering proxy

1. Verify mDNS/Avahi is running
2. Check firewall allows TCP port 6053
3. Manually add via ESPHome integration using IP:6053

### Build errors

1. Ensure pkg-config can find dependencies:
   ```bash
   pkg-config --modversion dbus-1 glib-2.0
   ```

2. For cross-compilation, verify sysroot has required libraries

## Performance

- **Memory usage**: ~2-4 MB RSS
- **CPU usage**: <1% idle, ~2-5% during scanning
- **Network**: Minimal (batched reports every 10s)
- **BLE scan rate**: Depends on BlueZ configuration

## License

MIT License - see LICENSE file

## Contributing

Contributions welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test on target hardware
5. Submit a pull request

## Credits

Developed for the [Thingino Project](https://github.com/themactep/thingino-firmware) - open-source firmware for Ingenic SoC cameras.

## Related Projects

- [ESPHome](https://esphome.io/) - Home automation framework
- [Home Assistant](https://www.home-assistant.io/) - Open source home automation
- [BlueZ](http://www.bluez.org/) - Official Linux Bluetooth stack
- [Thingino](https://thingino.com/) - IP camera firmware
