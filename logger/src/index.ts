/* eslint-disable no-console */
const noble = require('@abandonware/noble');
import { getPrisma } from './db-helper';

const NUS_SERVICE_UUID = '6e400001b5a3f393e0a9e50e24dcca9e';
const NUS_CHAR_RX_UUID = '6e400002b5a3f393e0a9e50e24dcca9e'; // write
const NUS_CHAR_TX_UUID = '6e400003b5a3f393e0a9e50e24dcca9e'; // notify

// Bridge framing (must match firmware):
// [0] [1]   [2] [3]         [ ... len ... ]   [len+4] [len+5]
// MAG_HI    MAG_LO          PAYLOAD(len)      CRC_HI   CRC_LO
// const BRIDGE_PACKET_MAGIC = 0xc03e;
// const MAGIC_HI = (BRIDGE_PACKET_MAGIC >> 8) & 0xff;
// const MAGIC_LO = BRIDGE_PACKET_MAGIC & 0xff;

// function fletcher16(data: Uint8Array, len: number): number {
//   let sum1 = 0;
//   let sum2 = 0;
//   for (let i = 0; i < len; i++) {
//     sum1 = (sum1 + data[i]) % 255;
//     sum2 = (sum2 + sum1) % 255;
//   }
//   return ((sum2 & 0xff) << 8) | (sum1 & 0xff);
// }

// Packet parsing constants from Packet.h
const PH_ROUTE_MASK = 0x03;
const PH_TYPE_SHIFT = 2;
const PH_TYPE_MASK = 0x0f;
const PH_VER_SHIFT = 6;
const PH_VER_MASK = 0x03;

const ROUTE_TYPES: { [key: number]: string } = {
  0x00: 'TRANSPORT_FLOOD',
  0x01: 'FLOOD',
  0x02: 'DIRECT',
  0x03: 'TRANSPORT_DIRECT',
};

const PAYLOAD_TYPES: { [key: number]: string } = {
  0x00: 'REQ',
  0x01: 'RESPONSE',
  0x02: 'TXT_MSG',
  0x03: 'ACK',
  0x04: 'ADVERT',
  0x05: 'GRP_TXT',
  0x06: 'GRP_DATA',
  0x07: 'ANON_REQ',
  0x08: 'PATH',
  0x09: 'TRACE',
  0x0a: 'MULTIPART',
  0x0f: 'RAW_CUSTOM',
};

interface ParsedPacket {
  header: {
    routeType: number;
    routeTypeName: string;
    payloadType: number;
    payloadTypeName: string;
    payloadVersion: number;
  };
  transportCodes?: {
    code1: number;
    code2: number;
  };
  pathLength: number;
  path?: number[];
  payloadLength: number;
  payload: {
    raw: string;
    parsed?: any;
  };
  snr?: number;
}

interface PacketInfo {
  rawData: Buffer;
  parsedPacket: ParsedPacket;
  deviceMac: string;
  rssi: number;
  timestamp: Date;
  readableText?: string;
}

interface PacketListener {
  onPacketReceived(packetInfo: PacketInfo): Promise<void> | void;
}

class PacketDistributor {
  private listeners: PacketListener[] = [];

  addListener(listener: PacketListener): void {
    this.listeners.push(listener);
  }

  removeListener(listener: PacketListener): void {
    const index = this.listeners.indexOf(listener);
    if (index > -1) {
      this.listeners.splice(index, 1);
    }
  }

  async distributePacket(packetInfo: PacketInfo): Promise<void> {
    const promises = this.listeners.map(listener => {
      try {
        return Promise.resolve(listener.onPacketReceived(packetInfo));
      } catch (error) {
        console.error('Error in packet listener:', error);
        return Promise.resolve();
      }
    });

    await Promise.allSettled(promises);
  }
}

class DatabaseInserter implements PacketListener {
  private prisma = getPrisma();

  async onPacketReceived(packetInfo: PacketInfo): Promise<void> {
    try {
      const { rawData, parsedPacket, deviceMac, rssi, timestamp, readableText } = packetInfo;
      
      // Convert path from numbers to hex strings
      const pathAsHexStrings = parsedPacket.path?.map(nodeHash =>
        nodeHash.toString(16).padStart(2, '0')
      ) || [];

      await this.prisma.meshPacket.create({
        data: {
          rawHex: rawData.toString('hex'),
          rawLength: rawData.length,
          deviceMac,
          rssi,
          snr: parsedPacket.snr,
          routeType: parsedPacket.header.routeType,
          routeTypeName: parsedPacket.header.routeTypeName,
          payloadType: parsedPacket.header.payloadType,
          payloadTypeName: parsedPacket.header.payloadTypeName,
          payloadVersion: parsedPacket.header.payloadVersion,
          transportCode1: parsedPacket.transportCodes?.code1,
          transportCode2: parsedPacket.transportCodes?.code2,
          pathLength: parsedPacket.pathLength,
          path: pathAsHexStrings,
          payloadLength: parsedPacket.payloadLength,
          payloadRaw: parsedPacket.payload.raw,
          payloadParsed: parsedPacket.payload.parsed,
          readableText,
        },
      });

      console.log('✓ Packet saved to database');
    } catch (error) {
      console.error('Failed to save packet to database:', error);
    }
  }
}

class ConsoleLogger implements PacketListener {
  onPacketReceived(packetInfo: PacketInfo): void {
    const { rawData, parsedPacket, deviceMac, rssi, timestamp, readableText } = packetInfo;
    
    console.log('=== MESH PACKET RECEIVED ===');
    console.log(`${timestamp.toISOString()} | device=${deviceMac} | rssi=${rssi} | payload_len=${rawData.length} | hex=${rawData.toString('hex')}`);
    console.log('Hex formatted:');
    console.log(hexPretty(rawData));
    
    if (readableText) {
      console.log(`ASCII: ${readableText}`);
    }
    
    console.log('=== PARSED PACKET ===');
    console.log(JSON.stringify(parsedPacket, null, 2));
    console.log('===============================');
  }
}

class MeshPacketParser {
  parsePacket(data: Buffer): ParsedPacket {
    let offset = 0;

    // Parse header (1 byte)
    const header = data[offset++];
    const routeType = header & PH_ROUTE_MASK;
    const payloadType = (header >> PH_TYPE_SHIFT) & PH_TYPE_MASK;
    const payloadVersion = (header >> PH_VER_SHIFT) & PH_VER_MASK;

    const result: ParsedPacket = {
      header: {
        routeType,
        routeTypeName: ROUTE_TYPES[routeType] || `UNKNOWN_${routeType}`,
        payloadType,
        payloadTypeName: PAYLOAD_TYPES[payloadType] || `UNKNOWN_${payloadType}`,
        payloadVersion,
      },
      pathLength: 0,
      payloadLength: 0,
      payload: {
        raw: '',
      },
    };

    // Parse transport codes if present (4 bytes, little endian)
    const hasTransportCodes = routeType === 0x00 || routeType === 0x03;
    if (hasTransportCodes && offset + 4 <= data.length) {
      const code1 = data.readUInt16LE(offset);
      const code2 = data.readUInt16LE(offset + 2);
      result.transportCodes = { code1, code2 };
      offset += 4;
    }

    // Parse path length (1 byte)
    if (offset < data.length) {
      result.pathLength = data[offset++];

      // Parse path (variable length)
      if (result.pathLength > 0 && offset + result.pathLength <= data.length) {
        result.path = [];
        for (let i = 0; i < result.pathLength; i++) {
          result.path.push(data[offset++]);
        }
      }
    }

    // Remaining data is payload
    if (offset < data.length) {
      const payloadData = data.slice(offset);
      result.payloadLength = payloadData.length;
      result.payload.raw = payloadData.toString('hex');

      // Parse payload based on type
      result.payload.parsed = this.parsePayload(payloadType, payloadData);
    }

    return result;
  }

  private parsePayload(payloadType: number, payloadData: Buffer): any {
    switch (payloadType) {
      case 0x04: // ADVERT
        return this.parseAdvertPayload(payloadData);
      case 0x02: // TXT_MSG
        return this.parseTextMessagePayload(payloadData);
      case 0x00: // REQ
      case 0x01: // RESPONSE
      case 0x08: // PATH
        return this.parseEncryptedPayload(payloadData);
      case 0x05: // GRP_TXT
      case 0x06: // GRP_DATA
        return this.parseGroupPayload(payloadData);
      default:
        return this.parseGenericPayload(payloadData);
    }
  }

  private parseAdvertPayload(data: Buffer): any {
    if (data.length < 36) return { error: 'Advertisement too short' };

    let offset = 0;
    // Public key (32 bytes)
    const publicKey = data.slice(offset, offset + 32).toString('hex');
    offset += 32;

    // Timestamp (4 bytes, little endian)
    const timestamp = data.readUInt32LE(offset);
    offset += 4;

    const result: any = {
      publicKey,
      timestamp,
      timestampDate: new Date(timestamp * 1000).toISOString(),
    };

    // Signature and app data (if present)
    if (data.length > offset) {
      const remaining = data.slice(offset);
      result.signatureAndAppData = remaining.toString('hex');

      // Try to extract readable text from end
      const text = this.extractReadableText(remaining);
      if (text) result.readableText = text;
    }

    return result;
  }

  private parseTextMessagePayload(data: Buffer): any {
    if (data.length < 4) return { error: 'Text message too short' };

    let offset = 0;
    // Destination hash (1 byte)
    const destHash = data[offset++];
    // Source hash (1 byte)
    const srcHash = data[offset++];
    // Cipher MAC (2 bytes)
    const cipherMac = data.readUInt16LE(offset);
    offset += 2;

    const result: any = {
      destinationHash: destHash.toString(16).padStart(2, '0'),
      sourceHash: srcHash.toString(16).padStart(2, '0'),
      cipherMac: cipherMac.toString(16).padStart(4, '0'),
    };

    // Ciphertext (rest of payload)
    if (data.length > offset) {
      const ciphertext = data.slice(offset);
      result.ciphertext = ciphertext.toString('hex');

      // Try to extract readable text
      const text = this.extractReadableText(ciphertext);
      if (text) result.readableText = text;
    }

    return result;
  }

  private parseEncryptedPayload(data: Buffer): any {
    if (data.length < 4) return { error: 'Encrypted payload too short' };

    let offset = 0;
    const destHash = data[offset++];
    const srcHash = data[offset++];
    const cipherMac = data.readUInt16LE(offset);
    offset += 2;

    const result: any = {
      destinationHash: destHash.toString(16).padStart(2, '0'),
      sourceHash: srcHash.toString(16).padStart(2, '0'),
      cipherMac: cipherMac.toString(16).padStart(4, '0'),
    };

    if (data.length > offset) {
      const ciphertext = data.slice(offset);
      result.ciphertext = ciphertext.toString('hex');

      const text = this.extractReadableText(ciphertext);
      if (text) result.readableText = text;
    }

    return result;
  }

  private parseGroupPayload(data: Buffer): any {
    if (data.length < 3) return { error: 'Group payload too short' };

    let offset = 0;
    const channelHash = data[offset++];
    const cipherMac = data.readUInt16LE(offset);
    offset += 2;

    const result: any = {
      channelHash: channelHash.toString(16).padStart(2, '0'),
      cipherMac: cipherMac.toString(16).padStart(4, '0'),
    };

    if (data.length > offset) {
      const ciphertext = data.slice(offset);
      result.ciphertext = ciphertext.toString('hex');

      const text = this.extractReadableText(ciphertext);
      if (text) result.readableText = text;
    }

    return result;
  }

  private parseGenericPayload(data: Buffer): any {
    const result: any = {
      data: data.toString('hex'),
    };

    const text = this.extractReadableText(data);
    if (text) result.readableText = text;

    return result;
  }

  private extractReadableText(data: Buffer): string | null {
    // Look for sequences of printable ASCII characters
    const ascii = data.toString('ascii');
    const readable = ascii.match(/[\x20-\x7E]{3,}/g);
    return readable ? readable.join(' ') : null;
  }
}

// Format hex in groups
function hexPretty(buf: Uint8Array): string {
  const hex = Buffer.from(buf).toString('hex');
  return hex.match(/.{1,2}/g)?.join(' ') ?? '';
}

function usageAndExit(): void {
  console.error('Usage: node dist/index.js <MAC_ADDRESS>');
  console.error('Example: node dist/index.js AA:BB:CC:DD:EE:FF');
  process.exit(1);
}

// Linux note: Run with proper permissions (e.g., sudo) or grant setcap for BLE.
async function main(): Promise<void> {
  const macArg = (process.argv[2] || '').trim();
  if (!macArg) usageAndExit();

  // normalize mac to lowercase
  const targetMac = macArg.toLowerCase();

  console.log(`Target MAC: ${targetMac}`);
  console.log('Initializing BLE...');

  const packetParser = new MeshPacketParser();
  const packetDistributor = new PacketDistributor();
  
  // Add listeners
  packetDistributor.addListener(new ConsoleLogger());
  packetDistributor.addListener(new DatabaseInserter());

  // Wait for adapter
  noble.on('stateChange', async (state: string) => {
    if (state !== 'poweredOn') {
      console.error(`BLE adapter state: ${state}. Waiting for poweredOn...`);
      return;
    }

    console.log('BLE poweredOn, starting scan for NUS service...');
    try {
      // filter is advisory; we will further check peripheral.address
      noble.startScanning([NUS_SERVICE_UUID], false);
    } catch (e) {
      console.error('Failed to start scanning:', e);
      process.exit(2);
    }
  });

  noble.on('discover', async (peripheral: any) => {
    const uniqueId = peripheral.id;
    const { localName } = peripheral.advertisement;
    console.log(
      `Discovered device: ${peripheral.address} (RSSI ${peripheral.rssi}) ${localName ? `Name: ${localName}` : ''} ID: ${uniqueId}`
    );
    const addr = peripheral.address ?? uniqueId;
    // Some stacks may report random address. If user provided full addr with colons, match exactly.
    if (true || addr === targetMac) {
      console.log(`Found target ${peripheral.address} (RSSI ${peripheral.rssi}) ${peripheral.MAC_ADDRESS}`);

      noble.stopScanning();

      try {
        await new Promise<void>((resolve, reject) => {
          peripheral.connect((error: any) => {
            if (error) reject(error);
            else resolve();
          });
        });

        console.log('Connected. Discovering services/characteristics...');

        const { services, characteristics } = await new Promise<any>((resolve, reject) => {
          peripheral.discoverSomeServicesAndCharacteristics(
            [NUS_SERVICE_UUID],
            [NUS_CHAR_TX_UUID, NUS_CHAR_RX_UUID],
            (error: any, services: any, characteristics: any) => {
              if (error) reject(error);
              else resolve({ services, characteristics });
            }
          );
        });

        if (!services.length) {
          throw new Error('NUS service not found on device');
        }

        // TX (notify) characteristic: device -> host notifications
        const txChar = characteristics.find((c: any) => c.uuid.toLowerCase() === NUS_CHAR_TX_UUID);
        // RX (write) characteristic: host -> device (unused here, but keep reference)
        const rxChar = characteristics.find((c: any) => c.uuid.toLowerCase() === NUS_CHAR_RX_UUID);

        if (!txChar) throw new Error('NUS TX characteristic not found');
        if (!rxChar) console.warn('NUS RX characteristic not found (continuing as receive-only)');

        await new Promise<void>((resolve, reject) => {
          txChar.subscribe((error: any) => {
            if (error) reject(error);
            else resolve();
          });
        });

        txChar.on('data', async (data: Buffer) => {
          console.log(`Received data from ${peripheral.address}: ${data.length} bytes`);

          try {
            // Parse the packet into structured data
            const parsedPacket = packetParser.parsePacket(data);
            
            // Extract readable text
            const ascii = data.toString('ascii').replace(/[^\x20-\x7E]/g, '.');
            const readableText = ascii.match(/[\x20-\x7E]{3,}/g)?.join(' ') || undefined;

            // Create packet info for distribution
            const packetInfo: PacketInfo = {
              rawData: data,
              parsedPacket,
              deviceMac: peripheral.address || peripheral.id,
              rssi: peripheral.rssi,
              timestamp: new Date(),
              readableText,
            };

            // Distribute to all listeners
            await packetDistributor.distributePacket(packetInfo);
            
          } catch (error) {
            console.log('=== PARSE ERROR ===');
            console.log(`Failed to parse packet: ${error}`);
          }
        });

        console.log('Subscribed to notifications. Printing received packets to stdout...');
        // Keep the process alive
        peripheral.on('disconnect', () => {
          console.error('Disconnected from device.');
          process.exit(0);
        });
      } catch (e) {
        console.error('Error during BLE operations:', e);
        process.exit(3);
      }
    }
  });
}

main().catch((e) => {
  console.error('Fatal error:', e);
  process.exit(99);
});
