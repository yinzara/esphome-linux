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