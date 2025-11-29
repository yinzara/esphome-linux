#!/usr/bin/env bash
set -e

LIBBLEPP_REPO="https://github.com/yinzara/libblepp.git"
LIBBLEPP_DIR="libblepp"
LIBBLEPP_TAG="v0.0.4"

# Clone libblepp repository if not already present
if [ ! -d "$LIBBLEPP_DIR" ]; then
    echo "Cloning libblepp repository..."
    git clone "$LIBBLEPP_REPO" "$LIBBLEPP_DIR"
    cd "$LIBBLEPP_DIR"
    echo "Checking out tag ${LIBBLEPP_TAG}..."
    git checkout "$LIBBLEPP_TAG"
    cd ..
else
    # Ensure we're on the correct tag
    cd "$LIBBLEPP_DIR"
    CURRENT_TAG=$(git describe --tags --exact-match 2>/dev/null || echo "")
    if [ "$CURRENT_TAG" != "$LIBBLEPP_TAG" ]; then
        echo "Checking out tag ${LIBBLEPP_TAG}..."
        git fetch --tags
        git checkout "$LIBBLEPP_TAG"
    else
        echo "Already on correct tag ${LIBBLEPP_TAG}"
    fi
    cd ..
fi

# Get the absolute paths to dependency output directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NIMBLE_ROOT="${SCRIPT_DIR}/../nimble/out"
BLUEZ_ROOT="${SCRIPT_DIR}/../bluez/out"

# Build libblepp with CMake
echo "Building libblepp..."
cd "${LIBBLEPP_DIR}"

# Check if CMakeLists.txt exists
if [ ! -f "CMakeLists.txt" ]; then
    echo "ERROR: CMakeLists.txt not found in ${LIBBLEPP_DIR}"
    echo "Contents of directory:"
    ls -la
    exit 1
fi

rm -rf build
mkdir -p build
cd build

# Check if we're cross-compiling (CC will be set to cross-compiler)
if [ -n "${CC}" ] && [ "${CC}" != "gcc" ] && [ "${CC}" != "cc" ]; then
    echo "Cross-compiling detected: CC=${CC}"
    # Configure with CMake for cross-compilation
    cmake \
        -DCMAKE_C_COMPILER="${CC}" \
        -DCMAKE_CXX_COMPILER="${CXX}" \
        -DCMAKE_AR="${AR}" \
        -DCMAKE_RANLIB="${RANLIB}" \
        -DCMAKE_STRIP="${STRIP}" \
        -DNIMBLE_ROOT="${NIMBLE_ROOT}" \
        -DBLUEZ_INCLUDE_DIR="${BLUEZ_ROOT}/include" \
        -DBLUEZ_LIBRARY="${BLUEZ_ROOT}/lib" \
        -DCMAKE_PREFIX_PATH="${BLUEZ_ROOT}/lib;${NIMBLE_ROOT}/lib" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="/usr" \
        -DWITH_SERVER_SUPPORT=ON \
        -DWITH_BLUEZ_SUPPORT=ON \
        -DWITH_NIMBLE_SUPPORT=ON \
        ..
else
    echo "Native compilation detected"
    # Configure with CMake for native compilation
    cmake \
        -DNIMBLE_ROOT="${NIMBLE_ROOT}" \
        -DBLUEZ_INCLUDE_DIR="${BLUEZ_ROOT}/include" \
        -DBLUEZ_LIBRARY="${BLUEZ_ROOT}/lib" \
        -DCMAKE_PREFIX_PATH="${BLUEZ_ROOT}/lib;${NIMBLE_ROOT}/lib" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="/usr" \
        -DWITH_SERVER_SUPPORT=ON \
        -DWITH_BLUEZ_SUPPORT=ON \
        -DWITH_NIMBLE_SUPPORT=ON \
        ..
fi

# Build and install
make -j$(nproc)
make install DESTDIR="${SCRIPT_DIR}/out"

echo "libblepp built and installed successfully"
echo "Verifying installation:"
ls -la "${SCRIPT_DIR}/out/lib/" 2>/dev/null || echo "Directory not found: ${SCRIPT_DIR}/out/lib/"
echo ""
find "${SCRIPT_DIR}/out" -name "*.so*" -o -name "*.a" 2>/dev/null || echo "No libraries found"
echo ""
echo "Expected library location: ${SCRIPT_DIR}/out/lib/libble++.so"
echo "Expected headers location: ${SCRIPT_DIR}/out/include/"