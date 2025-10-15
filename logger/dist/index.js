"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
/* eslint-disable no-console */
const noble = __importStar(require("@abandonware/noble"));
const NUS_SERVICE_UUID = '6e400001b5a3f393e0a9e50e24dcca9e';
const NUS_CHAR_RX_UUID = '6e400002b5a3f393e0a9e50e24dcca9e'; // write
const NUS_CHAR_TX_UUID = '6e400003b5a3f393e0a9e50e24dcca9e'; // notify
// Bridge framing (must match firmware):
// [0] [1]   [2] [3]         [ ... len ... ]   [len+4] [len+5]
// MAG_HI    MAG_LO          PAYLOAD(len)      CRC_HI   CRC_LO
const BRIDGE_PACKET_MAGIC = 0xC03E;
const MAGIC_HI = (BRIDGE_PACKET_MAGIC >> 8) & 0xff;
const MAGIC_LO = BRIDGE_PACKET_MAGIC & 0xff;
function fletcher16(data, len) {
    let sum1 = 0;
    let sum2 = 0;
    for (let i = 0; i < len; i++) {
        sum1 = (sum1 + data[i]) % 255;
        sum2 = (sum2 + sum1) % 255;
    }
    return ((sum2 & 0xff) << 8) | (sum1 & 0xff);
}
class BridgeParser {
    constructor() {
        this.rx = [];
        this.pos = 0;
        this.expectedLen = 0;
    }
    feed(chunk, onFrame) {
        for (let i = 0; i < chunk.length; i++) {
            const b = chunk[i];
            switch (this.pos) {
                case 0:
                    if (b === MAGIC_HI) {
                        this.rx[this.pos++] = b;
                    }
                    break;
                case 1:
                    if (b === MAGIC_LO) {
                        this.rx[this.pos++] = b;
                    }
                    else {
                        this.reset();
                    }
                    break;
                case 2: // LEN_HI
                    this.rx[this.pos++] = b;
                    break;
                case 3: // LEN_LO
                    this.rx[this.pos++] = b;
                    this.expectedLen = ((this.rx[2] & 0xff) << 8) | (this.rx[3] & 0xff);
                    // sanity: basic upper bound (Mesh packet wire length <= 255)
                    if (this.expectedLen <= 0 || this.expectedLen > 255) {
                        this.reset();
                    }
                    break;
                default:
                    this.rx[this.pos++] = b;
                    // Full frame when we have header(2) + len(2) + payload + crc(2)
                    if (this.pos === 4 + this.expectedLen + 2) {
                        const crcHi = this.rx[4 + this.expectedLen] & 0xff;
                        const crcLo = this.rx[5 + this.expectedLen] & 0xff;
                        const rcvCrc = (crcHi << 8) | crcLo;
                        // validate CRC over payload only
                        const payload = new Uint8Array(this.expectedLen);
                        for (let j = 0; j < this.expectedLen; j++)
                            payload[j] = this.rx[4 + j] & 0xff;
                        const calc = fletcher16(payload, payload.length);
                        if (calc === rcvCrc) {
                            onFrame(payload);
                        }
                        else {
                            const payloadHex = Buffer.from(payload).toString('hex');
                            console.error(`[WARN] checksum mismatch: calc=0x${calc.toString(16)} rcv=0x${rcvCrc.toString(16)} payload=${payloadHex}`);
                        }
                        this.reset();
                    }
                    break;
            }
        }
    }
    reset() {
        this.rx.length = 0;
        this.pos = 0;
        this.expectedLen = 0;
    }
}
// Format hex in groups
function hexPretty(buf) {
    var _a, _b;
    const hex = Buffer.from(buf).toString('hex');
    return (_b = (_a = hex.match(/.{1,2}/g)) === null || _a === void 0 ? void 0 : _a.join(' ')) !== null && _b !== void 0 ? _b : '';
}
function usageAndExit() {
    console.error('Usage: node dist/index.js <MAC_ADDRESS>');
    console.error('Example: node dist/index.js AA:BB:CC:DD:EE:FF');
    process.exit(1);
}
// Linux note: Run with proper permissions (e.g., sudo) or grant setcap for BLE.
async function main() {
    const macArg = (process.argv[2] || '').trim();
    if (!macArg)
        usageAndExit();
    // normalize mac to lowercase
    const targetMac = macArg.toLowerCase();
    console.log(`Target MAC: ${targetMac}`);
    console.log('Initializing BLE...');
    // Wait for adapter
    noble.on('stateChange', async (state) => {
        if (state !== 'poweredOn') {
            console.error(`BLE adapter state: ${state}. Waiting for poweredOn...`);
            return;
        }
        console.log('BLE poweredOn, starting scan for NUS service...');
        try {
            // filter is advisory; we will further check peripheral.address
            await noble.startScanningAsync([NUS_SERVICE_UUID], false);
        }
        catch (e) {
            console.error('Failed to start scanning:', e);
            process.exit(2);
        }
    });
    const parser = new BridgeParser();
    noble.on('discover', async (peripheral) => {
        const addr = (peripheral.address || '').toLowerCase();
        // Some stacks may report random address. If user provided full addr with colons, match exactly.
        if (addr === targetMac) {
            console.log(`Found target ${addr} (RSSI ${peripheral.rssi}) — stopping scan and connecting...`);
            noble.stopScanning();
            try {
                await peripheral.connectAsync();
                console.log('Connected. Discovering services/characteristics...');
                const { services, characteristics } = await peripheral.discoverSomeServicesAndCharacteristicsAsync([NUS_SERVICE_UUID], [NUS_CHAR_TX_UUID, NUS_CHAR_RX_UUID]);
                if (!services.length) {
                    throw new Error('NUS service not found on device');
                }
                // TX (notify) characteristic: device -> host notifications
                const txChar = characteristics.find((c) => c.uuid.toLowerCase() === NUS_CHAR_TX_UUID);
                // RX (write) characteristic: host -> device (unused here, but keep reference)
                const rxChar = characteristics.find((c) => c.uuid.toLowerCase() === NUS_CHAR_RX_UUID);
                if (!txChar)
                    throw new Error('NUS TX characteristic not found');
                if (!rxChar)
                    console.warn('NUS RX characteristic not found (continuing as receive-only)');
                await txChar.subscribeAsync();
                txChar.on('data', (data) => {
                    // Feed any size
                    parser.feed(data, (payload) => {
                        // payload is a serialized Mesh packet as produced by Packet::writeTo()
                        // For logging, print both compact and pretty hex
                        const now = new Date().toISOString();
                        const hex = Buffer.from(payload).toString('hex');
                        const pretty = hexPretty(payload);
                        console.log(`${now} | payload_len=${payload.length} | hex=${hex}`);
                        console.log(pretty);
                    });
                });
                console.log('Subscribed to notifications. Printing received packets to stdout...');
                // Keep the process alive
                peripheral.on('disconnect', () => {
                    console.error('Disconnected from device.');
                    process.exit(0);
                });
            }
            catch (e) {
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
