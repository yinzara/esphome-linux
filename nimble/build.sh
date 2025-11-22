#!/usr/bin/env bash
set -e

ATBM_REPO="https://github.com/gtxaspec/atbm-wifi.git"
ATBM_DIR="atbm-wifi"
ATBM_COMMIT="728e236d26a64907959b5b511af0b0fd8b87c509"

# Clone atbm-wifi repository if not already present
if [ ! -d "$ATBM_DIR" ]; then
    echo "Cloning atbm-wifi repository..."
    git clone "$ATBM_REPO" "$ATBM_DIR"
    cd "$ATBM_DIR"
    echo "Checking out commit ${ATBM_COMMIT}..."
    git checkout "$ATBM_COMMIT"
    cd ..
else
    # Ensure we're on the correct commit
    cd "$ATBM_DIR"
    CURRENT_COMMIT=$(git rev-parse HEAD)
    if [ "$CURRENT_COMMIT" != "$ATBM_COMMIT" ]; then
        echo "Checking out commit ${ATBM_COMMIT}..."
        git fetch
        git checkout "$ATBM_COMMIT"
    else
        echo "Already on correct commit ${ATBM_COMMIT}"
    fi
    cd ..
fi

# Copy Makefile and OS layer files into the atbm-wifi directory
echo "Copying Makefile to ${ATBM_DIR}..."
cp Makefile "${ATBM_DIR}/"

echo "Copying OS layer files to ${ATBM_DIR}/os/..."
mkdir -p "${ATBM_DIR}/os"
cp -r os/* "${ATBM_DIR}/os/"

# Build libnimble inside the source directory
echo "Building libnimble..."
cd "${ATBM_DIR}"
make clean
make
make install-staging DESTDIR=../out

echo "NimBLE library built and installed successfully"
echo "Library: $(pwd)/../out/lib/libnimble.so"
echo "Headers: $(pwd)/../out/include/"
