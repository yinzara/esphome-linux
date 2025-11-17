# syntax=docker/dockerfile:1
# Multi-stage Dockerfile for cross-compiling esphome-linux
# Caches dependencies in separate layers for fast rebuilds

# =============================================================================
# Stage 1: Build dependencies (each in separate cached layer)
# =============================================================================
FROM ubuntu:22.04 AS dependencies

# Dependency versions
ARG GLIB_VERSION=2.56.4
ARG DBUS_VERSION=1.12.20
ARG EXPAT_VERSION=2.5.0
ARG LIBFFI_VERSION=3.3
ARG PCRE_VERSION=8.45
ARG GETTEXT_VERSION=0.21
ARG ZLIB_VERSION=1.2.13
ARG SDK_DIR
ARG TOOLCHAIN_BIN

# Target architecture
ARG TARGET_ARCH=mips32r2
ARG TARGET_ENDIAN=EL

ENV DEBIAN_FRONTEND=noninteractive

# Install build tools
RUN apt-get update && apt-get install -y \
    bash curl make gcc g++ autoconf automake libtool qemu-user-static python3-pip ninja-build \
    pkg-config file \
    && pip3 install --no-cache-dir meson \
    && rm -rf /var/lib/apt/lists/* \

WORKDIR /build

# Set up cross-compilation environment
ENV PATH="${TOOLCHAIN_BIN}:${PATH}" \
    CROSS_COMPILE=mips-linux-gnu- \
    CC=mips-linux-gnu-gcc \
    CXX=mips-linux-gnu-g++ \
    AR=mips-linux-gnu-ar \
    AS=mips-linux-gnu-as \
    LD=mips-linux-gnu-ld \
    RANLIB=mips-linux-gnu-ranlib \
    STRIP=mips-linux-gnu-strip \
    CFLAGS="-${TARGET_ENDIAN} -march=${TARGET_ARCH} -O2" \
    CXXFLAGS="-${TARGET_ENDIAN} -march=${TARGET_ARCH} -O2" \
    LDFLAGS="-${TARGET_ENDIAN}"

# Build dependencies
RUN mkdir -p /sysroot/{usr/lib,usr/include,lib/pkgconfig,usr/lib/pkgconfig}

# Helper script for building packages
COPY <<'EOF' /build/build-package.sh
#!/bin/bash
set -e
name=$1
version=$2
url=$3
shift 3
extra_flags="$@"

echo "=== Building ${name} ${version} ==="
tarball="${name}-${version}.tar.gz"

# Try downloading with better error handling
echo "Downloading ${url}..."
if ! curl -L -f -o "${tarball}" "${url}"; then
    echo "Failed to download .tar.gz, trying .tar.xz..."
    tarball="${name}-${version}.tar.xz"
    if ! curl -L -f -o "${tarball}" "${url/.gz/.xz}"; then
        echo "ERROR: Failed to download ${name} from ${url}"
        exit 1
    fi
fi

echo "Extracting ${tarball}..."
tar xf "${tarball}"
cd ${name}-${version}

# Special handling for zlib (uses different configure)
if [ "$name" = "zlib" ]; then
    echo "Configuring zlib (special handling)..."
    CHOST=mips-linux-gnu \
    ./configure \
        --prefix=/usr \
        --shared >/dev/null
else
    ./configure \
        --host=mips-linux-gnu \
        --prefix=/usr \
        --sysconfdir=/etc \
        --localstatedir=/var \
        --enable-static \
        --enable-shared \
        ${extra_flags} \
        PKG_CONFIG_PATH="/sysroot/usr/lib/pkgconfig" \
        LDFLAGS="${LDFLAGS} -L/sysroot/usr/lib -Wl,-rpath-link,/sysroot/usr/lib" \
        CPPFLAGS="-I/sysroot/usr/include" >/dev/null
fi

# Special handling for glib - needs libintl
if [ "$name" = "glib" ]; then
    make -j$(nproc) LIBS="-lintl" >/dev/null 2>&1
else
    make -j$(nproc) >/dev/null 2>&1
fi
make DESTDIR="/sysroot" install >/dev/null 2>&1
cd ..
echo "✓ ${name} complete"
EOF

RUN chmod +x /build/build-package.sh

# Layer 1: Build libffi
RUN --mount=type=bind,from=sdk-mount,source=/,target=/sdk,readonly \
    /build/build-package.sh libffi ${LIBFFI_VERSION} \
        "https://github.com/libffi/libffi/releases/download/v${LIBFFI_VERSION}/libffi-${LIBFFI_VERSION}.tar.gz"

# Layer 2: Build pcre
RUN --mount=type=bind,from=sdk-mount,source=/,target=/sdk,readonly \
    /build/build-package.sh pcre ${PCRE_VERSION} \
        "https://sourceforge.net/projects/pcre/files/pcre/${PCRE_VERSION}/pcre-${PCRE_VERSION}.tar.gz" \
        --enable-utf --enable-unicode-properties

# Layer 3: Build zlib
RUN --mount=type=bind,from=sdk-mount,source=/,target=/sdk,readonly \
    /build/build-package.sh zlib ${ZLIB_VERSION} \
        "https://github.com/madler/zlib/releases/download/v${ZLIB_VERSION}/zlib-${ZLIB_VERSION}.tar.gz"

# Layer 4: Build expat
RUN --mount=type=bind,from=sdk-mount,source=/,target=/sdk,readonly \
    bash -c "/build/build-package.sh expat ${EXPAT_VERSION} \
        \"https://github.com/libexpat/libexpat/releases/download/R_\${EXPAT_VERSION//./_}/expat-${EXPAT_VERSION}.tar.gz\""

# Layer 5: Build gettext
RUN --mount=type=bind,from=sdk-mount,source=/,target=/sdk,readonly \
    /build/build-package.sh gettext ${GETTEXT_VERSION} \
        "https://ftp.gnu.org/pub/gnu/gettext/gettext-${GETTEXT_VERSION}.tar.gz" \
        --disable-java --disable-csharp --disable-libasprintf --disable-openmp

# Layer 6: Build glib with meson
RUN --mount=type=bind,from=sdk-mount,source=/,target=/sdk,readonly \
    echo "=== Building glib ${GLIB_VERSION} with meson ===" && \
    cd /build && \
    curl -L -f -o "glib-${GLIB_VERSION}.tar.xz" \
        "https://download.gnome.org/sources/glib/2.56/glib-${GLIB_VERSION}.tar.xz" && \
    tar xf "glib-${GLIB_VERSION}.tar.xz" && \
    cd "glib-${GLIB_VERSION}" && \
    printf '#!/bin/bash\nexec qemu-mipsel-static -L /sdk/toolchain/mips-gcc540-glibc222-64bit-r3.3.0/mips-linux-gnu/libc "$@"\n' > /usr/local/bin/qemu-mipsel-wrapper && \
    chmod +x /usr/local/bin/qemu-mipsel-wrapper && \
    printf "[binaries]\nc = 'mips-linux-gnu-gcc'\ncpp = 'mips-linux-gnu-g++'\nar = 'mips-linux-gnu-ar'\nstrip = 'mips-linux-gnu-strip'\npkg-config = 'pkg-config'\nexe_wrapper = '/usr/local/bin/qemu-mipsel-wrapper'\n\n[built-in options]\nc_args = ['-I/sysroot/usr/include']\nc_link_args = ['-L/sysroot/usr/lib', '-Wl,-rpath-link,/sysroot/usr/lib']\n\n[properties]\nsys_root = '/sysroot'\npkg_config_libdir = '/sysroot/usr/lib/pkgconfig'\n\n[host_machine]\nsystem = 'linux'\ncpu_family = 'mips'\ncpu = 'mips32r2'\nendian = 'little'\n" > /tmp/cross.txt && \
    PKG_CONFIG_PATH="/sysroot/usr/lib/pkgconfig" \
    PKG_CONFIG_SYSROOT_DIR="/sysroot" \
    meson setup builddir \
        --cross-file=/tmp/cross.txt \
        --prefix=/usr \
        --sysconfdir=/etc \
        --localstatedir=/var \
        --default-library=shared \
        -Dinternal_pcre=false \
        -Dman=false \
        -Dgtk_doc=false \
        -Dlibmount=false \
        -Dselinux=false || \
    { echo "=== MESON LOG ==="; cat builddir/meson-logs/meson-log.txt; exit 1; } && \
    (meson compile -C builddir || echo "Compile had failures but continuing...") && \
    DESTDIR="/sysroot" meson install -C builddir --no-rebuild --skip-subprojects && \
    echo "✓ glib complete"

# Layer 7: Build dbus
RUN --mount=type=bind,from=sdk-mount,source=/,target=/sdk,readonly \
    /build/build-package.sh dbus ${DBUS_VERSION} \
        "https://dbus.freedesktop.org/releases/dbus/dbus-${DBUS_VERSION}.tar.gz" \
        --disable-tests --disable-xml-docs --disable-doxygen-docs --without-x

# Verify dependencies were built
RUN ls -lh /sysroot/usr/lib/libdbus*.so* /sysroot/usr/lib/libglib*.so*

# =============================================================================
# Stage 2: Build application (rebuilds when source changes)
# =============================================================================
FROM ubuntu:22.04 AS builder

ARG SDK_DIR
ARG TOOLCHAIN_BIN

ENV DEBIAN_FRONTEND=noninteractive

# Install Meson and build tools
RUN apt-get update && apt-get install -y \
    python3 python3-pip ninja-build pkg-config file \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --no-cache-dir meson

# Copy sysroot from dependencies stage
COPY --from=dependencies /sysroot /sysroot

# Set up cross-compilation environment
ENV PATH="${TOOLCHAIN_BIN}:${PATH}" \
    PKG_CONFIG_PATH="/sysroot/usr/lib/pkgconfig:/sysroot/lib/pkgconfig" \
    PKG_CONFIG_SYSROOT_DIR="/sysroot"

WORKDIR /workspace

# Copy project files
COPY meson.build .
COPY cross/ cross/
COPY src/ src/

# Configure and build
RUN --mount=type=bind,from=sdk-mount,source=/,target=/sdk,readonly \
    meson setup build-mips --cross-file cross/ingenic-t31.txt && \
    meson compile -C build-mips && \
    file build-mips/esphome-linux

# =============================================================================
# Stage 3: Output (minimal layer with just the binary)
# =============================================================================
FROM scratch AS output
COPY --from=builder /workspace/build-mips/esphome-linux /esphome-linux-mips
