# Dockerfile for building esphome-linux
FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    ninja-build \
    pkg-config \
    gcc \
    libdbus-1-dev \
    libglib2.0-dev \
    && rm -rf /var/lib/apt/lists/*

# Install latest Meson via pip (apt version is 0.61.2, we need >= 0.62.0)
RUN pip3 install --no-cache-dir meson

# Set working directory
WORKDIR /workspace

# Copy project files
COPY . .

# Set TMPDIR to avoid macOS volume mount issues
ENV TMPDIR=/tmp

# Build the project
RUN meson setup build && \
    meson compile -C build

# Runtime stage (optional, for smaller image)
FROM ubuntu:22.04 AS runtime

RUN apt-get update && apt-get install -y \
    libdbus-1-3 \
    libglib2.0-0 \
    bluez \
    && rm -rf /var/lib/apt/lists/*

COPY --from=0 /workspace/build/esphome-linux /usr/local/bin/

EXPOSE 6053

CMD ["esphome-linux"]