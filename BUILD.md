# Building ESPHome BlueZ BLE Proxy

This document explains how to build the ESPHome BlueZ BLE Proxy for different architectures.

## Supported Architectures

- **x86_64** (Linux AMD64)
- **ARM64** (Linux ARM64)
- **MIPS** (Ingenic T31)

## Quick Start

### Using the Build Script

The easiest way to build is using the provided build script:

```bash
# Build for all architectures
./scripts/build.sh --all

# Build specific architecture
./scripts/build.sh --x86_64   # For x86_64
./scripts/build.sh --arm64    # For ARM64
./scripts/build.sh --mips     # For Ingenic T31 MIPS
./scripts/build.sh --docker   # Native Docker image

# Build for native architecture (default)
./scripts/build.sh
OR
./scripts/build.sh --native
```

### Individual Build Commands

#### x86_64 Build

```bash
docker buildx build --platform linux/amd64 \
    -f Dockerfile \
    --output type=local,dest=./output \
    .
```

#### ARM64 Build

```bash
docker buildx build --platform linux/arm64 \
    -f Dockerfile \
    --output type=local,dest=./output \
    .
```

#### MIPS Build (Ingenic T31)

```bash
Run the mips build which will downloading the Ingenic SDK if needed.
./scripts/build.sh --mips
```

## Detailed Build Instructions

### Prerequisites

#### For Native Builds (x86_64, ARM64)

- Docker with buildx support
- QEMU (for cross-platform builds)

On Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install docker.io qemu-user-static
docker buildx create --use
```

On macOS:
```bash
# Docker Desktop includes buildx and QEMU support
brew install docker
```

#### For MIPS Cross-Compilation

- Docker
- unrar (for extracting SDK)
- curl

On Ubuntu/Debian:
```bash
sudo apt-get install docker.io unrar curl
```

On macOS:
```bash
brew install docker rar
```

### Ingenic T31 MIPS Cross-Compilation

The MIPS build uses the Ingenic T31 SDK toolchain. The setup script automates the download and extraction:

This happens automatically if you use `./scripts/build.sh --mips`

#### SDK Setup

```bash
./scripts/setup-ingenic-sdk.sh
```

This script will:
1. Download 7 RAR files from the Ingenic SDK repository
2. Extract the toolchain using unrar
3. Unpack the cross-compiler
4. Verify the installation

The SDK will be installed to: `../Ingenic-SDK-T31-1.1.1-20200508/`

#### Manual SDK Setup

If you prefer to download manually:

```bash
# Create SDK directory
mkdir -p ../Ingenic-SDK-T31-1.1.1-20200508
cd ../Ingenic-SDK-T31-1.1.1-20200508

# Download RAR parts
for i in {1..7}; do
    curl -L -O "https://github.com/cgrrty/Ingenic-SDK-T31-1.1.1-20200508/raw/refs/heads/main/toolchain/toolchain.part${i}.rar"
done

# Extract toolchain
unrar x toolchain.part1.rar

# Extract compiler tarball
cd toolchain/gcc_540
tar xzf mips-gcc540-glibc222-64bit-r3.3.0.tar.gz
mv mips-gcc540-glibc222-64bit-r3.3.0 ../
cd ../..
```

#### Cross-Compilation Process

The `build-ingenic-t31.sh` script uses Docker multi-stage builds with layer caching:

**Stage 1: Build Dependencies** (cached layer)
- libffi
- pcre
- expat
- gettext
- glib
- dbus

**Stage 2: Build Application**
- Compiles esphome-linux using meson

**Benefits:**
- First build: ~30-40 minutes (builds all dependencies)
- Subsequent builds: ~1-2 minutes (uses cached dependency layers)
- Only rebuilds when source code changes

## GitHub Actions CI/CD

The project includes automated builds via GitHub Actions:

### Workflow File

`.github/workflows/build.yml`

### Triggered On

- Push to `main` branch
- Pull requests
- Manual workflow dispatch

### Jobs

1. **build-native**: Builds x86_64 and ARM64 using Docker buildx
2. **build-mips**: Cross-compiles for Ingenic T31 MIPS
3. **release**: Creates release bundle with all binaries

### Artifacts

After each successful build, artifacts are uploaded:
- `esphome-linux-x86_64`
- `esphome-linux-arm64`
- `esphome-linux-mips`

### Dockerhub

After each successful build the native Docker images are pushed to Dockerhub:
- `yinzara/esphome-linux` (linux/arm64 and linux/amd64 architectures)

### SDK Caching

The GitHub Actions workflow caches the Ingenic SDK to speed up builds:
- First run: Downloads and caches SDK (~10 minutes)
- Subsequent runs: Uses cached SDK (~30 seconds)

## Build Customization

### Environment Variables

#### MIPS Build

```bash
# Custom SDK location
export SDK_DIR=/path/to/sdk
./scripts/build-ingenic-t31.sh

# Custom toolchain
export TOOLCHAIN_NAME=mips-gcc540-glibc222-64bit-r3.3.0
./scripts/build-ingenic-t31.sh

# Custom architecture
export TARGET_ARCH=mips32r2
export TARGET_ENDIAN=EL
./scripts/build-ingenic-t31.sh
```

## Verifying Builds

### Check Binary Type

```bash
# x86_64
file ./esphome-linux-x86_64
# Output: ELF 64-bit LSB executable, x86-64

# ARM64
file ./esphome-linux-arm64
# Output: ELF 64-bit LSB executable, ARM aarch64

# MIPS
file ./esphome-linux-mips
# Output: ELF 32-bit LSB executable, MIPS, MIPS32 rel2
```

### Check Dependencies

```bash
# x86_64/ARM64
ldd ./esphome-linux-x86_64

# MIPS (using SDK readelf)
../Ingenic-SDK-T31-1.1.1-20200508/toolchain/mips-gcc540-glibc222-64bit-r3.3.0/bin/mips-linux-gnu-readelf \
    -d ./esphome-linux-mips | grep NEEDED
```

## Troubleshooting

### Docker Permission Denied

```bash
# Add user to docker group
sudo usermod -aG docker $USER
newgrp docker
```

### QEMU Not Installed

```bash
# Install QEMU user static
sudo apt-get install qemu-user-static
```

### unrar Not Found

```bash
# Ubuntu/Debian
sudo apt-get install unrar

# macOS
brew install rar

# RHEL/CentOS
sudo yum install unrar
```

### MIPS Build "C compiler cannot create executables"

This usually means the toolchain wasn't set up correctly:

```bash
# Verify SDK installation
ls -la ../Ingenic-SDK-T31-1.1.1-20200508/toolchain/mips-gcc540-glibc222-64bit-r3.3.0/bin/

# Test compiler
../Ingenic-SDK-T31-1.1.1-20200508/toolchain/mips-gcc540-glibc222-64bit-r3.3.0/bin/mips-linux-gnu-gcc --version

# Clean and rebuild
rm -rf ../Ingenic-SDK-T31-1.1.1-20200508
./scripts/setup-ingenic-sdk.sh
./scripts/build-ingenic-t31.sh
```

### Docker Build Cache Issues

```bash
# Clear Docker cache
docker builder prune -af

# Force rebuild without cache
docker build --no-cache -f cross.Dockerfile .
```

## Development

### Iterative Development for MIPS

When developing, the Docker layer caching significantly speeds up builds:

1. **First build**: All dependencies are built (~5 min)
2. **Code changes**: Only application layer rebuilds (~30 sec or less)
3. **Dependency changes**: Only affected layers rebuild

Example workflow:
```bash
# Make code changes in src/
vim src/esphome_api.c

# Rebuild (uses cached dependencies)
./scripts/build-ingenic-t31.sh  # Takes ~1-2 minutes
```

### Cleaning Build Artifacts

```bash
# Remove binaries
rm -f esphome-linux-*
OR
make clean

# Remove Docker images
docker image rm esphome-linux

# Remove SDK (if needed)
rm -rf ../Ingenic-SDK-T31-1.1.1-20200508
```

## Contributing

When contributing build system changes:

1. Test all architectures locally
2. Update this BUILD.md if adding new options
3. Verify GitHub Actions workflow still works
4. Test SDK caching behavior

## Additional Resources

- [Docker Buildx Documentation](https://docs.docker.com/buildx/working-with-buildx/)
- [Meson Cross-compilation](https://mesonbuild.com/Cross-compilation.html)
- [Ingenic T31 Documentation](https://github.com/cgrrty/Ingenic-SDK-T31-1.1.1-20200508)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
