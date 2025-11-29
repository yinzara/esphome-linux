# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.0.1] - 2025-01-17

### Added
- Initial release of ESPHome Linux proxy
- ESPHome Native API server implementation
- Plugin architecture for extensibility
  - Plugin registration system with auto-registration via GCC constructor attributes
  - Plugin hooks for device info configuration and entity listing
  - Generic plugin messaging API for broadcast and client-specific messages
- Bluetooth Proxy plugin
  - BLE advertisement scanning via BlueZ/D-Bus
  - Advertisement batching and periodic flushing (250ms intervals)
  - Support for passive scanning and raw advertisement data
  - Feature flag advertisement (BLE_FEATURE_PASSIVE_SCAN, BLE_FEATURE_RAW_ADVERTISEMENTS)
- Multi-architecture support
  - x86_64 (Linux AMD64)
  - ARM64 (Linux aarch64)
  - MIPS (Ingenic T31 MIPS32r2 little-endian)
- Build system
  - Meson build configuration with optional Bluetooth support
  - Docker-based multi-architecture builds
  - Cross-compilation support for Ingenic T31
  - GitHub Actions CI/CD pipeline
- Device info response with full ESPHome API protocol compliance
  - All fields from api.proto DeviceInfoResponse (fields 1-24)
  - Support for Bluetooth, Voice Assistant, and Z-Wave feature flags
  - API encryption support indication
- Documentation
  - Plugin architecture overview (PLUGIN_ARCHITECTURE.md)
  - Plugin development guide (PLUGIN_DEVELOPMENT.md)
  - Plugin hooks reference (PLUGIN_HOOKS.md)
  - BLE refactoring documentation (BLE_REFACTORING.md)
  - Build instructions (BUILD.md)
  - Buildroot integration guide (BUILDROOT_INTEGRATION.md)

### Changed
- N/A (initial release)

### Deprecated
- N/A (initial release)

### Removed
- N/A (initial release)

### Fixed
- N/A (initial release)

### Security
- N/A (initial release)

[0.0.1]: https://github.com/yinzara/esphome-linux/releases/tag/v0.0.1

## [0.0.2] - 2025-01-17

### Added
- N/A (initial release)

### Changed
- meson.build to use relative plugin paths

### Deprecated
- N/A (initial release)

### Removed
- N/A (initial release)

### Fixed
- N/A (initial release)

### Security
- N/A (initial release)

[0.0.2]: https://github.com/yinzara/esphome-linux/releases/tag/v0.0.2

## [0.0.3] - 2025-01-17

### Added
- N/A (initial release)

### Changed
- meson.build to support deps in plugins

### Deprecated
- N/A (initial release)

### Removed
- N/A (initial release)

### Fixed
- N/A (initial release)

### Security
- N/A (initial release)

[0.0.3]: https://github.com/yinzara/esphome-linux/releases/tag/v0.0.3

## [0.0.4] - 2025-01-18

### Added
- C++ plugin support

### Changed
- N/A

### Deprecated
- N/A

### Removed
- N/A

### Fixed
- Plugin implementation and proper protobuf header numbering of messages

### Security
- N/A

[0.0.4]: https://github.com/yinzara/esphome-linux/releases/tag/v0.0.4

## [0.0.5] - 2025-01-18

### Added
- N/A

### Changed
- N/A

### Deprecated
- N/A

### Removed
- N/A

### Fixed
- Shutdown handling

### Security
- N/A

[0.0.5]: https://github.com/yinzara/esphome-linux/releases/tag/v0.0.5

## [0.0.6] - 2025-01-18

### Added
- N/A

### Changed
- Dbus/libz dependencies replaced with libble++ and releasing dependencies now

### Deprecated
- N/A

### Removed
- N/A

### Fixed
- N/A

### Security
- N/A

[0.0.6]: https://github.com/yinzara/esphome-linux/releases/tag/v0.0.6

## [0.0.7] - 2025-01-22

### Added
- Dockerhub release of images

### Changed
- N/A

### Deprecated
- N/A

### Removed
- N/A

### Fixed
- N/A

### Security
- N/A

[0.0.7]: https://github.com/yinzara/esphome-linux/releases/tag/v0.0.7

## [0.0.8] - 2025-01-22

### Added
- LOG_LEVEL environment variable support for LibBLE++

### Changed
- N/A

### Deprecated
- N/A

### Removed
- N/A

### Fixed
- HCI/BlueZ support fixed in LibBLE++ dependency

### Security
- N/A

[0.0.8]: https://github.com/yinzara/esphome-linux/releases/tag/v0.0.8##

[0.0.9] - 2025-01-23

### Added
- N/A

### Changed
- Updated ESPHome version to 2025.12.0

### Deprecated
- N/A

### Removed
- N/A

### Fixed
- N/A

### Security
- N/A

[0.0.9]: https://github.com/yinzara/esphome-linux/releases/tag/v0.0.9

[0.0.10] - 2025-01-28

### Added
- N/A

### Changed
- Send actual raw advertisement data to HA

### Deprecated
- N/A

### Removed
- N/A

### Fixed
- N/A

### Security
- N/A

[0.0.10]: https://github.com/yinzara/esphome-linux/releases/tag/v0.0.10

[0.0.11] - 2025-01-28

### Added
- N/A

### Changed
- Added handle_subscribe_states_request() for subscribe states callback

### Deprecated
- N/A

### Removed
- N/A

### Fixed
- N/A

### Security
- N/A

[0.0.11]: https://github.com/yinzara/esphome-linux/releases/tag/v0.0.11
