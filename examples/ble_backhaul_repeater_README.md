# BLE Backhaul Repeater Example

This example demonstrates how to set up two repeaters linked via Bluetooth Low Energy backhaul.

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

## Operation

1. Power on both devices
2. They will automatically discover each other via BLE advertising
3. Once connected, packets are automatically forwarded between devices
4. If connection is lost, advertising restarts automatically
5. The mesh network sees this as a single repeater with combined coverage

## Advantages over ESP-NOW

- Lower power consumption (better for solar installations)
- No additional hardware required (no RS232 cables)
- No water ingress points (fully wireless)
- Works with both ESP32 and nRF52 platforms

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
- Connection quality information