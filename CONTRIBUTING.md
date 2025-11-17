# Contributing to ESPHome BlueZ BLE Proxy

Thank you for considering contributing! This project aims to be a lightweight, portable BLE proxy for embedded Linux devices.

## Development Setup

### Prerequisites

- Linux development environment (VM or native)
- GCC or Clang with C11 support
- Meson and Ninja
- D-Bus development libraries
- GLib development libraries
- BlueZ for testing

### Setting Up

```bash
# Clone the repository
git clone https://github.com/yourusername/esphome-linux.git
cd esphome-linux

# Install dependencies (Debian/Ubuntu)
sudo apt-get install meson ninja-build pkg-config \
  libdbus-1-dev libglib2.0-dev bluez

# Build
make

# Run
./esphome-linux
```

## Code Style

- **C11 standard** with POSIX extensions
- **4 spaces** for indentation (no tabs)
- **snake_case** for functions and variables
- **UPPER_CASE** for macros and constants
- **Clear comments** for complex logic
- **Doxygen-style** documentation for public APIs

## Testing

### Unit Testing

Currently, testing is manual. Automated tests are welcome contributions!

### Integration Testing

1. Run on target hardware (or QEMU)
2. Verify BlueZ integration
3. Test with Home Assistant
4. Check memory leaks with valgrind
5. Profile performance

### Test Checklist

- [ ] Native compilation works
- [ ] Cross-compilation works (MIPS, ARM)
- [ ] BlueZ D-Bus integration functional
- [ ] ESPHome protocol handshake succeeds
- [ ] BLE advertisements forwarded correctly
- [ ] Multi-client support works
- [ ] No memory leaks (valgrind clean)
- [ ] Graceful shutdown on SIGTERM/SIGINT

## Architecture Guidelines

### Module Responsibilities

- **main.c** - Entry point, signal handling, initialization
- **plugins** - Plugins for all functionality supported by the service
- **esphome_api.c** - ESPHome protocol server, client management
- **esphome_proto.c** - Protobuf encoding/decoding (lightweight)

### Design Principles

1. **Minimal dependencies** - Only D-Bus, GLib, pthread
2. **No dynamic allocation in hot paths** - Pre-allocate buffers
3. **Thread-safe** - Proper mutex usage for shared state
4. **Portable** - POSIX compliance, avoid platform-specific code
5. **Efficient** - Batch operations, avoid unnecessary wakeups
6. **Robust** - Handle errors gracefully, log issues

## Adding Features

### Before Starting

1. Open an issue to discuss the feature
2. Ensure it aligns with project goals
3. Check if similar functionality exists

### Feature Development

1. Create a feature branch: `git checkout -b feature/your-feature`
2. Implement with tests
3. Update documentation (README, comments)
4. Verify cross-compilation still works
5. Submit pull request

## Pull Request Process

1. **Fork** the repository
2. **Create** a feature branch
3. **Commit** with clear messages
4. **Test** on real hardware if possible
5. **Update** documentation
6. **Submit** PR with description

### PR Description Template

```markdown
## Description
Brief description of changes

## Motivation
Why this change is needed

## Testing
How you tested this change

## Checklist
- [ ] Compiles without warnings
- [ ] Cross-compilation tested
- [ ] Documentation updated
- [ ] No memory leaks
- [ ] Tested with Home Assistant
```

## Code Review

- Be respectful and constructive
- Focus on code quality and maintainability
- Suggest improvements, don't demand
- Acknowledge good work

## Versioning

This project uses [Semantic Versioning](https://semver.org/):

- **MAJOR** - Incompatible API changes
- **MINOR** - New features, backwards-compatible
- **PATCH** - Bug fixes

## Release Process

1. Update version in `meson.build`
2. Update CHANGELOG
3. Tag release: `git tag -a v0.x.x -m "Release 0.x.x"`
4. Push tag: `git push origin v0.x.x`
5. GitHub Actions builds artifacts (if configured)

## Questions?

- Open an issue for bugs
- Start a discussion for questions
- Join the Thingino Discord for chat

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
