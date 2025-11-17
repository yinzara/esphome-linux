# Buildroot Package Integration Guide

This guide explains how to integrate ESPHome Linux as a buildroot package, including support for custom plugins.

## Overview

Once integrated into Thingino's buildroot:
- Dependencies (dbus, glib) are handled automatically by buildroot
- The package builds as part of the firmware image
- Binary is included in the rootfs
- Service files can be installed automatically
- **Plugins can be added to extend functionality** (MediaPlayer, VoiceAssistant, etc.)

## Package Structure

Create the following structure in Thingino buildroot:

```
package/esphome-linux/
├── Config.in
├── esphome-linux.mk
└── S99esphome-linux (optional init script)
```

## Config.in

```makefile
config BR2_PACKAGE_ESPHOME_LINUX
    bool "esphome-linux"
    depends on BR2_USE_MMU  # meson needs MMU
    depends on BR2_TOOLCHAIN_HAS_THREADS
    select BR2_PACKAGE_DBUS
    select BR2_PACKAGE_LIBGLIB2
    help
      ESPHome Linux - lightweight TCP service implementing ESPHome Native API
      for Home Assistant integration on embedded Linux devices.

      Includes Bluetooth Proxy plugin by default. Extensible via plugin system.

      https://github.com/yourusername/esphome-linux

comment "esphome-linux needs a toolchain w/ threads"
    depends on !BR2_TOOLCHAIN_HAS_THREADS
```

## esphome-linux.mk

```makefile
################################################################################
#
# esphome-linux
#
################################################################################

ESPHOME_LINUX_VERSION = 1.0.0
ESPHOME_LINUX_SITE = $(call github,yourusername,esphome-linux,v$(ESPHOME_LINUX_VERSION))
ESPHOME_LINUX_LICENSE = MIT
ESPHOME_LINUX_LICENSE_FILES = LICENSE
ESPHOME_LINUX_DEPENDENCIES = dbus libglib2

# Use meson build system
ESPHOME_LINUX_CONF_OPTS = \
    -Dbuildtype=release

# Optional: Disable bluetooth proxy plugin
# ESPHOME_LINUX_CONF_OPTS += -Denable_bluetooth_proxy=false

$(eval $(meson-package))
```

## Optional: Init Script

`package/esphome-linux/S99esphome-linux`:

```bash
#!/bin/sh

NAME=esphome-linux
DAEMON=/usr/bin/$NAME
PIDFILE=/var/run/$NAME.pid

start() {
    printf "Starting $NAME: "

    # Check if bluetoothd is running
    if ! pgrep bluetoothd > /dev/null; then
        echo "FAILED (bluetoothd not running)"
        return 1
    fi

    start-stop-daemon -S -q -b -m -p $PIDFILE -x $DAEMON
    [ $? = 0 ] && echo "OK" || echo "FAIL"
}

stop() {
    printf "Stopping $NAME: "
    start-stop-daemon -K -q -p $PIDFILE
    [ $? = 0 ] && echo "OK" || echo "FAIL"
}

restart() {
    stop
    sleep 1
    start
}

case "$1" in
    start|stop|restart)
        "$1"
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac
```

To install the init script, add to `esphome-linux.mk`:

```makefile
define ESPHOME_LINUX_INSTALL_INIT_SYSV
    $(INSTALL) -D -m 755 package/esphome-linux/S99esphome-linux \
        $(TARGET_DIR)/etc/init.d/S99esphome-linux
endef
```

## Integration Steps

### 1. Add to Your Repository

```bash
cd /path/to/your-repo
mkdir -p package/esphome-linux
# Copy files from above
```

### 2. Add to Package Menu

Edit `package/Config.in` and add:

```makefile
menu "Networking applications"
    ...
    source "package/esphome-linux/Config.in"
    ...
endmenu
```

### 3. Enable in Configuration

Modify the appropriate device's defconfig to include `BR2_PACKAGE_ESPHOME_LINUX=y``

### 4. Build Firmware

```bash
make
```

The package will:
1. Download source from GitHub
2. Build using Meson with the configured toolchain
3. Install to target rootfs
4. Include in firmware image

## Testing the Package

### Build Just the Package

```bash
make esphome-linux
```

### Rebuild After Changes

```bash
make esphome-linux-rebuild
```

### Check Build Log

```bash
tail -f output/build/esphome-linux-*/build.log
```

### Extract from Firmware

```bash
# After building firmware
ls output/target/usr/bin/esphome-linux
```

## Buildroot Dependency Resolution

Buildroot automatically handles:
- **dbus**: Built from `package/dbus/`
- **libglib2**: Built from `package/libglib2/`
- **Cross-compilation**: Uses configured toolchain
- **Staging**: Libraries available in `output/staging/`
- **Target**: Binaries/libraries copied to `output/target/`

When `make` runs:
1. Builds dbus and glib first (dependencies)
2. Configures esphome-linux with PKG_CONFIG pointing to staging
3. Links against staging libraries
4. Installs to target rootfs

## Advantages of Buildroot Integration

✅ **Automatic dependency management** - No manual library building
✅ **Reproducible builds** - Same toolchain, same result
✅ **Version management** - Control exact versions of dependencies
✅ **Firmware integration** - Included in ROM image automatically
✅ **Init system** - Service starts on boot
✅ **Updates** - Easy to update via buildroot

## Development Workflow

### During Development

Integrate into buildroot:

```bash
# In Thingino firmware repo
make rebuild-esphome-linux
make
# Flash firmware to device
```

## Example: Complete Integration

1. **Create package directory**:
   ```bash
   mkdir -p package/esphome-linux
   ```

2. **Add files**: Config.in, esphome-linux.mk, S99esphome-linux

3. **Update Config.in**:
   ```bash
   echo 'source "package/esphome-linux/Config.in"' >> package/Config.in
   ```

4. **Configure**:
   ```bash
   make menuconfig
   # Enable esphome-linux
   ```

5. **Build**:
   ```bash
   make esphome-linux
   ```

6. **Test on device**:
   ```bash
   # Copy from output/target/usr/bin/esphome-linux
   scp output/target/usr/bin/esphome-linux root@camera:/usr/bin/
   ssh root@camera '/etc/init.d/S99esphome-linux start'
   ```

## Adding Custom Plugins via Buildroot

### Option 1: Fork and Add Plugins

Fork this repository and add your plugins to the `plugins/` directory:

```bash
# In your fork
mkdir -p plugins/my_feature
# Add your_feature_plugin.c

# Reference your fork in buildroot package
ESPHOME_LINUX_SITE = $(call github,yourusername,esphome-linux-extended,v$(ESPHOME_LINUX_VERSION))
```

### Option 2: Buildroot Override Directory

Use buildroot's override mechanism to inject plugins:

1. Create an override directory in your buildroot config:
   ```bash
   mkdir -p package/esphome-linux/plugins/my_feature
   ```

2. Add to `esphome-linux.mk`:
   ```makefile
   define ESPHOME_LINUX_COPY_PLUGINS
       if [ -d "$(BR2_EXTERNAL_THINGINO_PATH)/package/esphome-linux/plugins" ]; then \
           cp -r $(BR2_EXTERNAL_THINGINO_PATH)/package/esphome-linux/plugins/* \
                 $(@D)/plugins/; \
       fi
   endef
   ESPHOME_LINUX_PRE_CONFIGURE_HOOKS += ESPHOME_LINUX_COPY_PLUGINS
   ```

3. Place your plugin source in the `plugins` directory in `package/esphome-linux`

### Disabling Plugins in Buildroot

To the Bluetooth Proxy plugin:

```makefile
ESPHOME_LINUX_CONF_OPTS = \
    -Dbuildtype=release \
    -Denable_bluetooth_proxy=false
```

## Example: Complete Buildroot Integration with Plugin

```
my-buildroot-project/
└── package/
    ├── esphome-linux/
    │   ├── Config.in
    │   ├── esphome-linux.mk
    │   └── S99esphome-linux
    ├── esphome-linux/plugins/my-plugin
    └────── my-plugin.c
```

## References

- [Buildroot Manual - Adding Packages](https://buildroot.org/downloads/manual/manual.html#adding-packages)
- [Buildroot Meson Infrastructure](https://buildroot.org/downloads/manual/manual.html#meson-package-tutorial)
- [Thingino Contribution Guide](https://github.com/themactep/thingino-firmware/blob/master/CONTRIBUTING.md)
- [Plugin Development Guide](docs/PLUGIN_DEVELOPMENT.md)
