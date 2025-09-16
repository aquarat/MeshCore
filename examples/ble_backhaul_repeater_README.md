# BLE Backhaul Repeater Example

This example demonstrates how to set up two repeaters linked via Bluetooth Low Energy backhaul with manual configuration.

## Use Case

You have two repeaters with different antenna configurations:
- Repeater A: Omnidirectional antenna for general coverage
- Repeater B: Highly directional antenna for long-range links

By linking them via BLE backhaul, they work as a single logical repeater:
1. Packets received by Repeater A are forwarded to Repeater B via BLE and retransmitted
2. Packets received by Repeater B are forwarded to Repeater A via BLE and retransmitted
3. This combines the coverage patterns of both antennas

## Hardware Requirements

- 2x ESP32 or nRF52-based devices
- BLE support on both devices
- Solar power compatibility (BLE is more energy efficient than ESP-NOW)

## Firmware Configuration

### ESP32-based devices:
```bash
# Compile BLE backhaul repeater firmware
pio run --environment Generic_BLE_Backhaul_repeatr

# Flash to both devices
pio run --environment Generic_BLE_Backhaul_repeatr --target upload
```

### nRF52-based devices:
```bash
# Compile nRF52 BLE backhaul repeater firmware
pio run --environment Generic_nRF52_BLE_Backhaul_repeatr

# Flash to both devices
pio run --environment Generic_nRF52_BLE_Backhaul_repeatr --target upload
```

## Configuration Commands

**NEW**: BLE backhaul supports manual configuration via console commands instead of automatic advertising.

### Set Target Device MAC Address
```
set ble.target XX:XX:XX:XX:XX:XX
```
Configure the MAC address of the target repeater to connect to.

### Configure BLE UUIDs
```
set ble.service.uuid 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
set ble.tx.uuid 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
set ble.rx.uuid 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
```

### Set BLE Power
```
set ble.tx.power 3
```
Set BLE transmit power (0-20).

### Enable/Disable Auto Advertising
```
set ble.auto.adv on   # Enable automatic advertising
set ble.auto.adv off  # Disable automatic advertising (manual pairing)
```

### Manual Connection Control
```
ble connect      # Connect to configured target
ble disconnect   # Disconnect BLE connection
ble status       # Show BLE connection status
```

## Setup Process

1. **Flash both devices** with BLE backhaul firmware
2. **Configure Device A** (e.g., omnidirectional antenna):
   ```
   set ble.target AA:BB:CC:DD:EE:FF    # MAC of Device B
   set ble.auto.adv on                 # Enable advertising
   ```
3. **Configure Device B** (e.g., directional antenna):
   ```
   set ble.target FF:EE:DD:CC:BB:AA    # MAC of Device A  
   set ble.auto.adv off                # Disable advertising
   ble connect                         # Connect to Device A
   ```
4. **Verify connection**: Use `ble status` on both devices

## Operation

- Devices automatically discover each other based on configured MAC addresses
- Packets are automatically forwarded between devices via BLE
- Connection automatically restarts if disconnected
- The mesh network sees this as a single repeater with combined coverage
- Manual pairing provides better control than automatic discovery

## Advantages over ESP-NOW

- **Manual Control**: Explicit pairing instead of automatic discovery
- **Lower Power**: Better for solar installations
- **No Additional Hardware**: No RS232 cables required
- **Water Resistant**: Fully wireless connection
- **Configurable**: UUIDs and power settings via console

## Debug Output

Enable debug logging to monitor BLE backhaul operation:

```cpp
// In platformio.ini build flags:
-D BLE_DEBUG_LOGGING=1
-D MESH_PACKET_LOGGING=1
-D MESH_DEBUG=1
```

Debug output shows:
- BLE connection/disconnection events
- Packet forwarding statistics
- Configuration changes
- Connection quality information

## Configuration Persistence

All BLE settings are saved to device preferences and restored on reboot:
- Target MAC address
- Service and characteristic UUIDs  
- BLE power setting
- Auto-advertising preference