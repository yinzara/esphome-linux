#!/bin/bash
# Script to download and extract Ingenic T31 SDK toolchain
# Can be run locally or in CI/CD

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SDK_BASE_URL="https://github.com/cgrrty/Ingenic-SDK-T31-1.1.1-20200508/raw/refs/heads/main/toolchain"
SDK_DIR="${SDK_DIR:-${PROJECT_ROOT}/../Ingenic-SDK-T31-1.1.1-20200508}"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}Setting up Ingenic T31 SDK toolchain${NC}"

# Check if SDK already exists
if [ -d "${SDK_DIR}/toolchain/mips-gcc540-glibc222-64bit-r3.3.0" ]; then
    echo -e "${YELLOW}SDK toolchain already exists at: ${SDK_DIR}${NC}"
    echo -e "${GREEN}Verifying toolchain...${NC}"
    if [ -f "${SDK_DIR}/toolchain/mips-gcc540-glibc222-64bit-r3.3.0/bin/mips-linux-gnu-gcc" ]; then
        echo -e "${GREEN}✓ Toolchain verified${NC}"
        exit 0
    else
        echo -e "${YELLOW}Toolchain incomplete, re-downloading...${NC}"
        rm -rf "${SDK_DIR}"
    fi
fi

# Create SDK directory
mkdir -p "${SDK_DIR}/toolchain"
cd "${SDK_DIR}/toolchain"

# Check for required tools
if ! command -v unrar &> /dev/null; then
    echo -e "${RED}Error: unrar not found${NC}"
    echo "Install unrar:"
    echo "  Ubuntu/Debian: sudo apt-get install unrar"
    echo "  macOS: brew install unrar"
    echo "  RHEL/CentOS: sudo yum install unrar"
    exit 1
fi

# Download RAR parts
echo -e "${GREEN}Downloading toolchain parts (7 files)...${NC}"
for i in {1..7}; do
    part_file="toolchain.part${i}.rar"
    if [ -f "${part_file}" ]; then
        echo -e "${YELLOW}${part_file} already exists, skipping...${NC}"
    else
        echo -e "${GREEN}Downloading ${part_file}...${NC}"
        curl -L -o "${part_file}" "${SDK_BASE_URL}/${part_file}" \
            || { echo -e "${RED}Failed to download ${part_file}${NC}"; exit 1; }
    fi
done

# Extract RAR archive (part1 will extract all parts)
echo -e "${GREEN}Extracting toolchain...${NC}"
unrar x -o+ toolchain.part1.rar \
    || { echo -e "${RED}Failed to extract toolchain${NC}"; exit 1; }

# Verify toolchain directory was created
if [ ! -d "gcc_540" ]; then
    echo -e "${RED}Error: gcc_540 not found after extraction${NC}"
    exit 1
fi

# Extract the actual toolchain tarball
echo -e "${GREEN}Extracting toolchain tarball...${NC}"
cd gcc_540
if [ -f "mips-gcc540-glibc222-64bit-r3.3.0.tar.gz" ]; then
    tar xzf mips-gcc540-glibc222-64bit-r3.3.0.tar.gz \
        || { echo -e "${RED}Failed to extract toolchain tarball${NC}"; exit 1; }
else
    echo -e "${RED}Error: mips-gcc540-glibc222-64bit-r3.3.0.tar.gz not found${NC}"
    exit 1
fi

# Move toolchain to expected location
cd ..
if [ -d "gcc_540/mips-gcc540-glibc222-64bit-r3.3.0" ]; then
    mv gcc_540/mips-gcc540-glibc222-64bit-r3.3.0 .
    rm -rf gcc_540
fi

# Verify toolchain
if [ -f "mips-gcc540-glibc222-64bit-r3.3.0/bin/mips-linux-gnu-gcc" ]; then
    echo -e "${GREEN}✓ Toolchain setup complete!${NC}"
    echo -e "${GREEN}SDK location: ${SDK_DIR}${NC}"
    echo -e "${GREEN}Toolchain: ${SDK_DIR}/toolchain/mips-gcc540-glibc222-64bit-r3.3.0${NC}"
else
    echo -e "${RED}Error: Toolchain not found at expected location${NC}"
    exit 1
fi

# Clean up RAR files (optional)
if [ "${CLEANUP_RAR:-yes}" = "yes" ]; then
    echo -e "${GREEN}Cleaning up RAR files...${NC}"
    cd "${SDK_DIR}/toolchain"
    rm -f toolchain.part*.rar
fi

echo -e "${GREEN}Setup complete!${NC}"
