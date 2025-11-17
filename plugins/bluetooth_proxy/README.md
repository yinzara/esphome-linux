# Bluetooth Proxy Plugin

Core plugin that provides Bluetooth LE scanning functionality via BlueZ D-Bus interface.

## Overview

This plugin:
- Scans for BLE advertisements using BlueZ
- Forwards advertisements to Home Assistant via ESPHome Native API
- Handles subscription/unsubscription requests from clients
- Caches and batches advertisements for efficient reporting

## Features

- **Passive BLE scanning** - Low power consumption
- **Advertisement caching** - Deduplicates and batches reports
- **Periodic reporting** - Reports devices every 10 seconds
- **Stale device removal** - Cleans up devices not seen for 60 seconds
- **Full advertisement data** - Manufacturer data, service UUIDs, service data

## Requirements

- BlueZ installed and running (`bluetoothd`)
- D-Bus system bus access
- Bluetooth adapter (hci0)

## ESPHome Messages Handled

- `ESPHOME_MSG_SUBSCRIBE_BLUETOOTH_LE_ADVERTISEMENTS_REQUEST` (66)
  - Starts BLE scanning
  - Client subscribes to advertisement stream

- `ESPHOME_MSG_UNSUBSCRIBE_BLUETOOTH_LE_ADVERTISEMENTS_REQUEST` (80)
  - Stops BLE scanning
  - Client unsubscribes from advertisements

## ESPHome Messages Sent

- `ESPHOME_MSG_BLUETOOTH_LE_RAW_ADVERTISEMENTS_RESPONSE` (93)
  - Contains BLE advertisement data
  - Sent periodically (every 10s) for discovered devices

## Architecture

```
BlueZ (bluetoothd)
      │
      ▼ D-Bus Signals
BLE Scanner (ble_scanner.c)
      │
      ▼ Callbacks
Bluetooth Proxy Plugin
      │
      ▼ ESPHome API
Home Assistant
```

## Files

- `bluetooth_proxy_plugin.c` - Plugin wrapper, handles ESPHome messages
- `ble_scanner.c` - BlueZ D-Bus integration, BLE scanning logic
- `ble_scanner.h` - BLE scanner API
- `README.md` - This file

## Configuration

No configuration needed - automatically starts when Home Assistant subscribes.

##Testing

```bash
# Check if BlueZ is running
systemctl status bluetooth

# Check for Bluetooth adapter
hciconfig

# Monitor D-Bus signals
dbus-monitor --system "interface='org.freedesktop.DBus.Properties'"
```

## Troubleshooting

**BLE scanner fails to initialize:**
- Check BlueZ is running: `systemctl status bluetooth`
- Check adapter exists: `hciconfig`
- Check D-Bus permissions

**No advertisements received:**
- Ensure BLE devices are advertising nearby
- Check BlueZ discovery: `bluetoothctl scan on`
- Check adapter is powered: `bluetoothctl power on`

## Future Enhancements

Potential additions to this plugin:
- GATT client operations (read/write characteristics)
- Device connection management
- Bluetooth pairing
- Multiple adapter support
- Advertisement filtering

## License

MIT - Same as main project
