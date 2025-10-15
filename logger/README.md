# BLE NUS Packet Logger

A TypeScript application that connects to MeshCore repeaters via Bluetooth Low Energy (BLE) Nordic UART Service (NUS)
and logs received mesh packets to stdout.

## Prerequisites

- Linux system with Bluetooth support
- Node.js 16+ and pnpm
- Proper BLE permissions (run with `sudo` or configure setcap permissions)

## Installation

```bash
cd logger
pnpm install
pnpm run build
```

## Usage

```bash
# Connect to a specific repeater by MAC address
node dist/index.js AA:BB:CC:DD:EE:FF

# Example with real MAC
node dist/index.js 12:34:56:78:9a:bc
```

## How it works

1. Scans for BLE devices advertising the Nordic UART Service (NUS)
2. Connects to the device matching the provided MAC address
3. Subscribes to NUS TX characteristic notifications
4. Parses the bridge framing protocol:
   - Magic header: `0xC03E`
   - Length field: 2 bytes
   - Payload: mesh packet data
   - Fletcher-16 checksum: 2 bytes
5. Validates checksum and prints valid mesh packets as hex

## Output Format

```
2025-09-18T21:30:00.000Z | payload_len=42 | hex=01040a1b2c3d...
01 04 0a 1b 2c 3d 4e 5f 60 71 82 93 a4 b5 c6 d7...
```

## Troubleshooting

- **Permission denied**: Run with `sudo` or configure BLE permissions
- **Device not found**: Ensure the repeater is advertising NUS and MAC is correct
- **Connection fails**: Check that the repeater has BLE backhaul enabled as peripheral

## Getting the repeater MAC

Connect to the repeater via USB serial console and run:

```
mac
```

This will print the device's MAC address from FICR.
