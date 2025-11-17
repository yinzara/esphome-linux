#!/bin/bash
# Cross-compilation build script for Ingenic T31 (MIPS)
# Uses Docker multi-stage build for fast rebuilds with cached dependencies

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK_DIR="${SDK_DIR:-$(cd "${SCRIPT_DIR}/../../Ingenic-SDK-T31-1.1.1-20200508" && pwd)}"

# Toolchain configuration
TOOLCHAIN_NAME="${TOOLCHAIN_NAME:-mips-gcc540-glibc222-64bit-r3.3.0}"
TOOLCHAIN_BIN="${SDK_DIR}/toolchain/${TOOLCHAIN_NAME}/bin"

# Target architecture
TARGET_ARCH="${TARGET_ARCH:-mips32r2}"
TARGET_ENDIAN="${TARGET_ENDIAN:-EL}"

# Dependency versions (build args for Dockerfile)
GLIB_VERSION="${GLIB_VERSION:-2.56.4}"
DBUS_VERSION="${DBUS_VERSION:-1.12.20}"
EXPAT_VERSION="${EXPAT_VERSION:-2.5.0}"
LIBFFI_VERSION="${LIBFFI_VERSION:-3.3}"
PCRE_VERSION="${PCRE_VERSION:-8.45}"
GETTEXT_VERSION="${GETTEXT_VERSION:-0.21}"
ZLIB_VERSION="${ZLIB_VERSION:-1.2.13}"

# Docker image tag
DOCKER_IMAGE="esphome-linux-cross"
DOCKER_TAG="${DOCKER_TAG:-latest}"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}Cross-compiling esphome-linux for Ingenic T31 (MIPS)${NC}"

# Verify SDK exists
if [ ! -d "${SDK_DIR}" ]; then
    echo -e "${RED}Error: Ingenic SDK not found at: ${SDK_DIR}${NC}"
    echo -e "${YELLOW}Set SDK_DIR environment variable or place SDK at ../Ingenic-SDK-T31-1.1.1-20200508${NC}"
    echo -e "${YELLOW}or run scripts/setup-ingenic-sdk.sh to install the appropriate files"
    exit 1
fi

echo -e "${GREEN}Using SDK: ${SDK_DIR}${NC}"
echo -e "${YELLOW}Building with Docker (dependencies cached in layers)...${NC}"

# Clean previous build
rm -rf "${SCRIPT_DIR}/esphome-linux-mips"

# Build with Docker using BuildKit (required for bind mounts)
echo -e "${GREEN}Building Docker image with multi-stage caching...${NC}"
DOCKER_BUILDKIT=1 docker build \
    --platform linux/amd64 \
    -f cross.Dockerfile \
    -t ${DOCKER_IMAGE}:${DOCKER_TAG} \
    --build-context sdk-mount="${SDK_DIR}" \
    --build-arg TOOLCHAIN_BIN="/sdk/toolchain/${TOOLCHAIN_NAME}/bin" \
    --build-arg TARGET_ARCH=${TARGET_ARCH} \
    --build-arg TARGET_ENDIAN=${TARGET_ENDIAN} \
    --build-arg GLIB_VERSION=${GLIB_VERSION} \
    --build-arg DBUS_VERSION=${DBUS_VERSION} \
    --build-arg EXPAT_VERSION=${EXPAT_VERSION} \
    --build-arg LIBFFI_VERSION=${LIBFFI_VERSION} \
    --build-arg PCRE_VERSION=${PCRE_VERSION} \
    --build-arg GETTEXT_VERSION=${GETTEXT_VERSION} \
    --build-arg ZLIB_VERSION=${ZLIB_VERSION} \
    --build-arg SDK_DIR="${SDK_DIR}" \
    --target builder \
    .

# Extract binary from the build stage
echo -e "${GREEN}Extracting binary...${NC}"
container_id=$(docker create ${DOCKER_IMAGE}:${DOCKER_TAG})
docker cp ${container_id}:/workspace/build-mips/esphome-linux "${SCRIPT_DIR}/../esphome-linux-mips"
docker rm ${container_id}

# Check if binary exists
if [ -f "${SCRIPT_DIR}/../esphome-linux-mips" ]; then
    echo -e "${GREEN}✓ Cross-compilation successful!${NC}"
    echo -e "Binary location: ${SCRIPT_DIR}/esphome-linux-mips"
    file "${SCRIPT_DIR}/../esphome-linux-mips"

    # Show dependencies
    echo -e "${GREEN}Dependencies:${NC}"
    docker run --rm \
        -v "${SDK_DIR}":/sdk:ro \
        -v "${SCRIPT_DIR}":/work \
        ${DOCKER_IMAGE}:${DOCKER_TAG} \
        /sdk/toolchain/mips-gcc540-glibc222-64bit-r3.3.0/bin/mips-linux-gnu-readelf -d /work/esphome-linux-mips | grep NEEDED || true
else
    echo -e "${RED}✗ Binary not found. Check the build output above.${NC}"
    exit 1
fi
