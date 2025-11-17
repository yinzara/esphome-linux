#!/bin/bash
# Build script for all architectures
# Can be run locally or in CI/CD

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

usage() {
    echo "Usage: $0 [OPTION]"
    echo "Build esphome-linux for different architectures"
    echo ""
    echo "Options:"
    echo "  --native         Build for native architecture (default)"
    echo "  --x86_64         Build for x86_64"
    echo "  --arm64          Build for ARM64"
    echo "  --mips           Build for Ingenic T31 MIPS"
    echo "  --all            Build for all architectures"
    echo "  --help           Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 --native      # Build for current architecture"
    echo "  $0 --mips        # Build for MIPS only"
    echo "  $0 --all         # Build all architectures"
}

build_native() {
    echo -e "${GREEN}Building for native architecture...${NC}"
    cd "${PROJECT_ROOT}"

    if command -v docker &> /dev/null; then
        echo -e "${BLUE}Using Docker build...${NC}"
        docker build -t esphome-linux:native -f Dockerfile .

        # Extract binary
        container_id=$(docker create esphome-linux:native)
        docker cp "${container_id}:/usr/local/bin/esphome-linux" ./esphome-linux
        docker rm "${container_id}"

        echo -e "${GREEN}✓ Native build complete: ./esphome-linux${NC}"
    else
        echo -e "${YELLOW}Docker not found, using local build...${NC}"
        if [ ! -d "build" ]; then
            meson setup build
        fi
        meson compile -C build
        cp build/esphome-linux ./esphome-linux
        echo -e "${GREEN}✓ Native build complete: ./esphome-linux${NC}"
    fi
}

build_x86_64() {
    echo -e "${GREEN}Building for x86_64...${NC}"
    cd "${PROJECT_ROOT}"

    docker buildx build --platform linux/amd64 \
        -f Dockerfile \
        --output type=local,dest=./output-amd64 \
        .

    if [ -f "./output-amd64/usr/local/bin/esphome-linux" ]; then
        mv ./output-amd64/usr/local/bin/esphome-linux ./esphome-linux-x86_64
        rm -rf ./output-amd64
        echo -e "${GREEN}✓ x86_64 build complete: ./esphome-linux-x86_64${NC}"
        file ./esphome-linux-x86_64
    else
        echo -e "${RED}✗ x86_64 build failed${NC}"
        return 1
    fi
}

build_arm64() {
    echo -e "${GREEN}Building for ARM64...${NC}"
    cd "${PROJECT_ROOT}"

    docker buildx build --platform linux/arm64 \
        -f Dockerfile \
        --output type=local,dest=./output-arm64 \
        .

    if [ -f "./output-arm64/usr/local/bin/esphome-linux" ]; then
        mv ./output-arm64/usr/local/bin/esphome-linux ./esphome-linux-arm64
        rm -rf ./output-arm64
        echo -e "${GREEN}✓ ARM64 build complete: ./esphome-linux-arm64${NC}"
        file ./esphome-linux-arm64
    else
        echo -e "${RED}✗ ARM64 build failed${NC}"
        return 1
    fi
}

build_mips() {
    echo -e "${GREEN}Building for Ingenic T31 MIPS...${NC}"
    cd "${PROJECT_ROOT}"

    # Check if SDK exists, if not download it
    if [ ! -d "../Ingenic-SDK-T31-1.1.1-20200508/toolchain/mips-gcc540-glibc222-64bit-r3.3.0" ]; then
        echo -e "${YELLOW}Ingenic SDK not found, downloading...${NC}"
        "${SCRIPT_DIR}/setup-ingenic-sdk.sh"
    fi

    # Run cross-compilation
    ${SCRIPT_DIR}/build-ingenic-t31.sh

    if [ -f "./esphome-linux-mips" ]; then
        echo -e "${GREEN}✓ MIPS build complete: ./esphome-linux-mips${NC}"
        file ./esphome-linux-mips
    else
        echo -e "${RED}✗ MIPS build failed${NC}"
        return 1
    fi
}

build_all() {
    echo -e "${BLUE}Building for all architectures...${NC}"

    # Get native architecture
    NATIVE_ARCH=$(uname -m)

    build_x86_64
    build_arm64
    build_mips

    # Only build native if it's not x86_64 or arm64
    if [ "$NATIVE_ARCH" != "x86_64" ] && [ "$NATIVE_ARCH" != "aarch64" ] && [ "$NATIVE_ARCH" != "arm64" ]; then
        echo -e "${BLUE}Also building for native architecture (${NATIVE_ARCH})...${NC}"
        build_native
    fi

    echo -e "${GREEN}All builds complete!${NC}"
    echo -e "${GREEN}Binaries:${NC}"
    ls -lh ./esphome-linux-* 2>/dev/null || ls -lh ./esphome-linux
}

# Parse command line arguments
if [ $# -eq 0 ]; then
    build_native
    exit 0
fi

case "$1" in
    --native)
        build_native
        ;;
    --x86_64)
        build_x86_64
        ;;
    --arm64)
        build_arm64
        ;;
    --mips)
        build_mips
        ;;
    --all)
        build_all
        ;;
    --help|-h)
        usage
        exit 0
        ;;
    *)
        echo -e "${RED}Unknown option: $1${NC}"
        usage
        exit 1
        ;;
esac
