# Convenience Makefile wrapper for Meson
# This makes the build process feel more traditional while using Meson underneath

BUILDDIR ?= build
CROSSFILE ?=
MESON_ARGS ?=

# Detect if cross-compiling via CROSS_COMPILE variable
ifdef CROSS_COMPILE
	CC := $(CROSS_COMPILE)gcc
	AR := $(CROSS_COMPILE)ar
	STRIP := $(CROSS_COMPILE)strip
	export CC AR STRIP
endif

.PHONY: all configure build clean install uninstall help cross

all: build

configure:
	@if [ -n "$(CROSSFILE)" ]; then \
		echo "Configuring with cross-file: $(CROSSFILE)"; \
		meson setup $(BUILDDIR) --cross-file $(CROSSFILE) $(MESON_ARGS); \
	else \
		echo "Configuring native build"; \
		meson setup $(BUILDDIR) $(MESON_ARGS); \
	fi

build: | configure
	@echo "Building..."
	meson compile -C $(BUILDDIR)

clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILDDIR)

install:
	@echo "Installing..."
	meson install -C $(BUILDDIR)

uninstall:
	@echo "Uninstalling..."
	ninja -C $(BUILDDIR) uninstall

test:
	@echo "Running tests..."
	meson test -C $(BUILDDIR)

reconfigure:
	@echo "Reconfiguring..."
	meson setup $(BUILDDIR) --reconfigure

cross:
	@echo "Cross-compiling for Ingenic T31 (MIPS)..."
	./scripts/build.sh --mips

help:
	@echo "ESPHome BlueZ BLE Proxy - Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Build the project (default)"
	@echo "  configure   - Configure the build"
	@echo "  build       - Compile the project"
	@echo "  clean       - Remove build artifacts"
	@echo "  install     - Install to system"
	@echo "  uninstall   - Remove from system"
	@echo "  reconfigure - Reconfigure existing build"
	@echo "  cross       - Cross-compile for Ingenic T31 (auto-builds deps)"
	@echo "  help        - Show this help"
	@echo ""
	@echo "Variables:"
	@echo "  BUILDDIR    - Build directory (default: build)"
	@echo "  CROSSFILE   - Meson cross-compilation file"
	@echo "  CROSS_COMPILE - Traditional cross-compile prefix (e.g., mipsel-linux-)"
	@echo "  MESON_ARGS  - Additional arguments to meson"
	@echo ""
	@echo "Examples:"
	@echo "  make                                    # Native build"
	@echo "  make cross                              # Cross-compile for Ingenic T31"
	@echo "  make CROSS_COMPILE=mipsel-linux-       # Cross-compile (generic)"
	@echo "  make CROSSFILE=cross/mips-linux.txt    # Cross-compile with file"
	@echo "  make install                            # Install to system"
	@echo "  make clean                              # Clean build"
