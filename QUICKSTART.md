# Quick Start Guide

Get up and running in 5 minutes!

## Prerequisites Check

```bash
# Verify dependencies
which meson ninja pkg-config gcc
pkg-config --modversion dbus-1 glib-2.0
systemctl status bluetooth  # Should be active
```

## Build & Run

### Native Build (x86/ARM/etc)

```bash
# Build
make

# Run (requires BlueZ running)
sudo ./build/esphome-ble-proxy
```

### Cross-Compile for Embedded Device (MIPS example)

```bash
# Method 1: Using CROSS_COMPILE
make clean
make CROSS_COMPILE=mipsel-linux-

# Method 2: Using cross-file
make clean
make CROSSFILE=cross/mips-linux.txt

# Copy to device
scp build/esphome-ble-proxy root@192.168.1.10:/usr/bin/
```

## Quick Test

### 1. Start the Service

```bash
./build/esphome-ble-proxy
```

You should see:
```
esphome-service v1.0.0 - ESPHome Bluetooth Proxy for Thingino
Device: your-hostname
MAC: XX:XX:XX:XX:XX:XX

ESPHome API server started successfully
Listening on port 6053
BLE scanner started successfully
```

### 2. Verify BLE Scanning

In another terminal:
```bash
# Check bluetoothd is scanning
bluetoothctl
> scan on

# Check D-Bus messages
dbus-monitor --system "interface=org.freedesktop.DBus.Properties"
```

### 3. Test ESPHome Connection

```bash
# Test connection (requires netcat)
nc localhost 6053

# Or use ESPHome CLI
pip install esphome
esphome run test.yaml  # Minimal config pointing to localhost:6053
```

### 4. Add to Home Assistant

**Option A: Auto-discovery (if mDNS working)**
- Go to Settings → Devices & Services
- ESPHome integration should show new device
- Click "Configure"

**Option B: Manual**
- Go to Settings → Devices & Services → Add Integration
- Select "ESPHome"
- Enter IP: `192.168.1.10` Port: `6053`
- No encryption, no password

## Install as System Service

```bash
# Copy binary
sudo cp build/esphome-ble-proxy /usr/local/bin/

# Install service
sudo cp esphome-ble-proxy.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable esphome-ble-proxy
sudo systemctl start esphome-ble-proxy

# Check status
sudo systemctl status esphome-ble-proxy
sudo journalctl -u esphome-ble-proxy -f
```

## Troubleshooting

### "Failed to connect to D-Bus"
```bash
# Start D-Bus
sudo systemctl start dbus

# Check permissions
groups  # Should include 'bluetooth' or run as root
```

### "Failed to start BLE scanning"
```bash
# Start bluetoothd
sudo systemctl start bluetooth

# Check adapter
hciconfig
bluetoothctl show
```

### "Port 6053 already in use"
```bash
# Find what's using it
sudo lsof -i :6053

# Kill it or change port in main.c
```

### No devices appearing in Home Assistant

```bash
# Check scanning is working
./build/esphome-ble-proxy  # Watch for "Reported X device(s)"

# Verify advertisements
hcitool lescan  # Should show devices quickly

# Check firewall
sudo ufw allow 6053/tcp
```

## Next Steps

- Read [README.md](README.md) for full documentation
- See [CONTRIBUTING.md](CONTRIBUTING.md) to contribute
- Configure report interval in `src/ble_scanner.c`
- Set up mDNS for auto-discovery
- Monitor performance with `top` or `htop`

## Example: Thingino Integration

For Thingino firmware cameras:

```bash
# On your build machine
make CROSS_COMPILE=mipsel-linux-

# Copy to camera
scp build/esphome-ble-proxy root@thingino-cam:/usr/bin/

# SSH to camera
ssh root@thingino-cam

# Create init script
cat > /etc/init.d/S95esphome-ble-proxy << 'EOF'
#!/bin/sh
case "$1" in
  start)
    /usr/bin/esphome-ble-proxy &
    ;;
  stop)
    killall esphome-ble-proxy
    ;;
esac
EOF

chmod +x /etc/init.d/S95esphome-ble-proxy

# Start
/etc/init.d/S95esphome-ble-proxy start
```

## Performance Expectations

- **Startup**: <1 second
- **Memory**: 2-4 MB RSS
- **CPU**: <1% idle, 2-5% scanning
- **Network**: ~1 KB/s (batched every 10s)
- **BLE scan rate**: Depends on BlueZ config

Enjoy your BLE proxy! 🎉
