#include "helpers/nrf52/BLENUSBridge.h"

#ifdef NRF52_PLATFORM

#include <Arduino.h>
#include <bluefruit.h>

static const uint8_t MAGIC_HI = (BridgeBase::BRIDGE_PACKET_MAGIC >> 8) & 0xFF;
static const uint8_t MAGIC_LO = (BridgeBase::BRIDGE_PACKET_MAGIC) & 0xFF;

BLENUSBridge* BLENUSBridge::_instance = nullptr;

// Central role objects from Adafruit core
static BLEClientUart clientUart;

// Static scan callback
void BLENUSBridge::scanCallback(ble_gap_evt_adv_report_t* report) {
  // If peer MAC configured, filter
  if (_instance && _instance->_enabled && _instance->_role == 1) {
    if (_instance->_peer_mac[0] | _instance->_peer_mac[1] | _instance->_peer_mac[2] |
        _instance->_peer_mac[3] | _instance->_peer_mac[4] | _instance->_peer_mac[5]) {
      // Mac in report is little-endian (addr[0]..addr[5])
      if (memcmp(report->peer_addr.addr, _instance->_peer_mac, 6) != 0) {
        return; // not our peer
      }
    }
    // Try to connect
    Bluefruit.Central.connect(report);
  }
}

void BLENUSBridge::connectCallback(uint16_t conn_handle) {
  (void) conn_handle;
  if (!_instance) return;

  _instance->_centralConnected = true;
  _instance->_centralReady = false;

  // Discover NUS on this connection
  if (clientUart.discover(conn_handle)) {
    clientUart.enableTXD();
    clientUart.setRxCallback(nullptr); // we'll poll in loop()
    _instance->_centralReady = true;
#if MESH_PACKET_LOGGING
    Serial.printf("%s: BLE NUS Central connected\n", _instance->getLogDateTime());
#endif
  } else {
#if MESH_PACKET_LOGGING
    Serial.printf("%s: BLE NUS Central discover failed\n", _instance->getLogDateTime());
#endif
    Bluefruit.disconnect(conn_handle);
  }
}

void BLENUSBridge::disconnectCallback(uint16_t conn_handle, uint8_t reason) {
  (void) conn_handle;
  if (!_instance) return;
  _instance->_centralConnected = false;
  _instance->_centralReady = false;
#if MESH_PACKET_LOGGING
  Serial.printf("%s: BLE NUS Central disconnected reason=%u\n", _instance->getLogDateTime(), reason);
#endif
  _instance->centralStartScan();
}

BLENUSBridge::BLENUSBridge(mesh::PacketManager* mgr, mesh::RTCClock* rtc, NodePrefs* prefs)
: BridgeBase(mgr, rtc), _prefs(prefs),
  _clientUart(&clientUart),
  _centralReady(false),
  _centralConnected(false),
  _rx_pos(0) {
  _instance = this;
  memset(_peer_mac, 0, sizeof(_peer_mac));
  cachePrefs();
}

void BLENUSBridge::cachePrefs() {
  _enabled = _prefs->ble_backhaul_enabled != 0;
  _role    = _prefs->ble_backhaul_role;           // 0=Peripheral 1=Central
  {
    int8_t tx = _prefs->ble_tx_power_dbm;
    if (tx < -40) tx = -40;
    if (tx > 8) tx = 8;
    _tx_power = tx;
  }
  memcpy(_peer_mac, _prefs->ble_peer_mac, sizeof(_peer_mac));
}

void BLENUSBridge::stopAll() {
  Bluefruit.Advertising.stop();
  if (Bluefruit.Central.connected()) {
    Bluefruit.disconnect(Bluefruit.connHandle());
  }
  Bluefruit.Scanner.stop();
}

void BLENUSBridge::initPeripheral() {
  // Setup NUS service
  _periphUart.begin();

  // Advertise NUS + name
  Bluefruit.Advertising.clearData();
  Bluefruit.ScanResponse.clearData();
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(_periphUart);
  Bluefruit.ScanResponse.addName();

  {
    // Intervals are in 0.625 ms units. Defaults: 5s..10s => (8000, 16000)
    uint16_t advMin = _prefs->ble_adv_itvl_min ? _prefs->ble_adv_itvl_min : 8000;
    uint16_t advMax = _prefs->ble_adv_itvl_max ? _prefs->ble_adv_itvl_max : 16000;
    Bluefruit.Advertising.setInterval(advMin, advMax);
  }
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);
#if MESH_PACKET_LOGGING
  Serial.printf("%s: BLE NUS Peripheral advertising\n", getLogDateTime());
#endif
}

void BLENUSBridge::centralStartScan() {
  Bluefruit.Scanner.stop();
  Bluefruit.Scanner.clearFilters();
  // Filter by NUS service to avoid connecting to random devices
  Bluefruit.Scanner.filterUuid(BLEUART_UUID_SERVICE);
  {
    // Defaults: continuous scanning with 3s window => (4800, 4800)
    uint16_t itvl = _prefs->ble_scan_itvl ? _prefs->ble_scan_itvl : 4800;
    uint16_t win  = _prefs->ble_scan_window ? _prefs->ble_scan_window : 4800;
    if (win > itvl) win = itvl; // Bluefruit requires window <= interval
    Bluefruit.Scanner.setInterval(itvl, win);
  }
  Bluefruit.Scanner.useActiveScan(true);
  Bluefruit.Scanner.setRxCallback(scanCallback);
  Bluefruit.Scanner.start(0); // forever
#if MESH_PACKET_LOGGING
  Serial.printf("%s: BLE Central scanning...\n", getLogDateTime());
#endif
}

void BLENUSBridge::initCentral() {
  _centralConnected = false;
  _centralReady = false;

  clientUart.begin();
  Bluefruit.Central.setConnectCallback(connectCallback);
  Bluefruit.Central.setDisconnectCallback(disconnectCallback);

  centralStartScan();
}

void BLENUSBridge::begin() {
  cachePrefs();

  if (!_enabled) return;

  if (!Bluefruit.begin()) {
#if MESH_PACKET_LOGGING
    Serial.printf("%s: BLE begin() failed\n", getLogDateTime());
#endif
    return;
  }

  // Set name and TX power (BLE name follows MeshCore node name)
  Bluefruit.setName(_prefs->node_name);
  Bluefruit.setTxPower(_tx_power);

  // Initialize according to role
  if (_role == 0) {       // Peripheral/Server
    initPeripheral();
  } else {                // Central/Client
    initCentral();
  }
}

void BLENUSBridge::reconfigure() {
  cachePrefs();
  stopAll();
  begin();
}

uint16_t BLENUSBridge::feedParser(uint8_t byte) {
  // State machine for: MAGIC_HI, MAGIC_LO, LEN_HI, LEN_LO, [payload ...], CRC_HI, CRC_LO
  static uint16_t expected_len = 0;

  switch (_rx_pos) {
    case 0:
      if (byte == MAGIC_HI) _rx_buffer[_rx_pos++] = byte;
      break;
    case 1:
      if (byte == MAGIC_LO) {
        _rx_buffer[_rx_pos++] = byte;
      } else {
        _rx_pos = 0;
      }
      break;
    case 2:
      _rx_buffer[_rx_pos++] = byte; // LEN_HI
      break;
    case 3:
      _rx_buffer[_rx_pos++] = byte; // LEN_LO
      expected_len = ((uint16_t)_rx_buffer[2] << 8) | _rx_buffer[3];
      if (expected_len > MAX_WIRE_LEN) {
        // invalid length, reset
        _rx_pos = 0;
      }
      break;
    default:
      if (_rx_pos < MAX_BLE_PACKET_SIZE) {
        _rx_buffer[_rx_pos++] = byte;
      } else {
        // overflow, reset
        _rx_pos = 0;
      }
      break;
  }

  // Full frame when we have header(2) + len(2) + payload(expected_len) + checksum(2)
  if (_rx_pos == (size_t)(SERIAL_OVERHEAD + expected_len)) {
    uint16_t crc = ((uint16_t)_rx_buffer[4 + expected_len] << 8) | _rx_buffer[5 + expected_len];
    bool ok = validateChecksum(_rx_buffer + 4, expected_len, crc);
    uint16_t complete_len = ok ? expected_len : 0;
    _rx_pos = 0; // reset state for next frame
    return complete_len;
  }
  return 0;
}

void BLENUSBridge::processParsedPacket(uint16_t len) {
  mesh::Packet* pkt = _mgr->allocNew();
  if (!pkt) return;

  if (pkt->readFrom(_rx_buffer + 4, (uint8_t)len)) {
    onPacketReceived(pkt);
  } else {
    _mgr->free(pkt);
  }
}

size_t BLENUSBridge::writeBytes(const uint8_t* data, size_t len) {
  if (_role == 0) {
    return _periphUart.write(data, len);
  }
  if (_centralConnected && _centralReady) {
    return clientUart.write(data, len);
  }
  return 0;
}

void BLENUSBridge::loop() {
  if (!_enabled) return;

  // RX: feed bytes from the active interface to the parser
  if (_role == 0) {
    // Peripheral
    int n = _periphUart.available();
    while (n-- > 0) {
      uint8_t b = _periphUart.read();
      uint16_t parsed = feedParser(b);
      if (parsed) {
        processParsedPacket(parsed);
      }
    }
  } else if (_centralConnected && _centralReady) {
    int n = clientUart.available();
    while (n-- > 0) {
      uint8_t b = clientUart.read();
      uint16_t parsed = feedParser(b);
      if (parsed) {
        processParsedPacket(parsed);
      }
    }
  }
}

void BLENUSBridge::onPacketReceived(mesh::Packet* packet) {
  handleReceivedPacket(packet);
}

void BLENUSBridge::onPacketTransmitted(mesh::Packet* packet) {
  if (!_enabled || !packet) return;

  if (!_seen_packets.hasSeen(packet)) {
    // Serialize packet
    uint8_t buf[(MAX_TRANS_UNIT + 1) + SERIAL_OVERHEAD];
    uint16_t len = packet->writeTo(buf + 4);

    if (len > (MAX_TRANS_UNIT + 1)) {
#if MESH_PACKET_LOGGING
      Serial.printf("%s: BLE BRIDGE: TX too large=%u\n", getLogDateTime(), (uint32_t)len);
#endif
      return;
    }

    // Header
    buf[0] = MAGIC_HI;
    buf[1] = MAGIC_LO;
    buf[2] = (len >> 8) & 0xFF;
    buf[3] = len & 0xFF;

    // CRC over payload
    uint16_t crc = fletcher16(buf + 4, len);
    buf[4 + len] = (crc >> 8) & 0xFF;
    buf[5 + len] = crc & 0xFF;

    size_t total = SERIAL_OVERHEAD + len;
    size_t written = writeBytes(buf, total);
#if MESH_PACKET_LOGGING
    if (written == total) {
      Serial.printf("%s: BLE BRIDGE: TX len=%u\n", getLogDateTime(), (uint32_t)len);
    } else {
      Serial.printf("%s: BLE BRIDGE: TX failed/w=%u\n", getLogDateTime(), (uint32_t)written);
    }
#endif
  }
}

#endif // NRF52_PLATFORM