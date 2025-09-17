#include "BLERadio.h"
#include <bluefruit.h>

static BLEService bleService(0x0001);
static BLECharacteristic bleTxChar(0x0003);
static BLECharacteristic bleRxChar(0x0002);
static bool deviceConnected = false;
static volatile bool is_send_complete = true;
static uint8_t rx_buf[256];
static uint8_t last_rx_len = 0;

// Configuration variables
static String target_mac_address = "";
static uint16_t service_uuid = 0x0001;
static uint16_t tx_char_uuid = 0x0003;
static uint16_t rx_char_uuid = 0x0002;
static bool auto_advertising_enabled = false;

static void connect_callback(uint16_t conn_handle) {
  deviceConnected = true;
  BLE_DEBUG_PRINTLN("Device connected");
}

static void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  deviceConnected = false;
  BLE_DEBUG_PRINTLN("Device disconnected, restarting advertising");
  Bluefruit.Advertising.start(0); // Start advertising indefinitely
}

static void rx_callback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
  BLE_DEBUG_PRINTLN("Received %d bytes", len);
  if (len <= sizeof(rx_buf)) {
    memcpy(rx_buf, data, len);
    last_rx_len = len;
  }
}

void BLERadio::init() {
  // Initialize Bluefruit
  Bluefruit.begin();
  
  // Set device name
  Bluefruit.setName("MeshCore-BLE-Backhaul");
  
  // Set connection callbacks
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  
  // Configure and start BLE service
  bleService.begin();
  
  // Configure TX characteristic (for sending data)
  bleTxChar.setProperties(CHR_PROPS_NOTIFY);
  bleTxChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  bleTxChar.setFixedLen(0); // Variable length
  bleTxChar.begin();
  
  // Configure RX characteristic (for receiving data)
  bleRxChar.setProperties(CHR_PROPS_WRITE | CHR_PROPS_WRITE_WO_RESP);
  bleRxChar.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  bleRxChar.setFixedLen(0); // Variable length
  bleRxChar.setWriteCallback(rx_callback);
  bleRxChar.begin();
  
  // Only start advertising if auto advertising is enabled
  if (auto_advertising_enabled) {
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addService(bleService);
    Bluefruit.Advertising.addName();
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
    Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
    Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
    BLE_DEBUG_PRINTLN("Auto-advertising started, waiting for BLE backhaul connection...");
  } else {
    BLE_DEBUG_PRINTLN("BLE backhaul initialized - manual pairing mode");
    if (!target_mac_address.length() > 0) {
      BLE_DEBUG_PRINTLN("Target MAC configured: %s", target_mac_address.c_str());
    }
  }
  
  is_send_complete = true;
}

void BLERadio::setTxPower(uint8_t dbm) {
  // nRF52 power levels: -40, -20, -16, -12, -8, -4, 0, +2, +3, +4, +5, +6, +7, +8 dBm
  int8_t power = (int8_t)dbm;
  if (power > 8) power = 8;
  if (power < -40) power = -40;
  Bluefruit.setTxPower(power);
}

uint32_t BLERadio::intID() {
  uint8_t mac[6];
  Bluefruit.getAddr(mac);
  uint32_t n, m;
  memcpy(&n, &mac[0], 4);
  memcpy(&m, &mac[2], 4);
  
  return n + m;
}

bool BLERadio::startSendRaw(const uint8_t* bytes, int len) {
  if (!deviceConnected) {
    BLE_DEBUG_PRINTLN("Send failed: not connected");
    return false;
  }

  // Send message via BLE notification
  is_send_complete = false;
  bool success = bleTxChar.notify((uint8_t*)bytes, len);
  
  if (success) {
    n_sent++;
    BLE_DEBUG_PRINTLN("Send success, len=%d", len);
  } else {
    BLE_DEBUG_PRINTLN("Send failed");
  }
  
  is_send_complete = true;  // BLE notifications are fire-and-forget
  return success;
}

bool BLERadio::isSendComplete() {
  return is_send_complete;
}

void BLERadio::onSendFinished() {
  is_send_complete = true;
}

bool BLERadio::isInRecvMode() const {
  return is_send_complete;    // if NO send in progress, then we're in Rx mode
}

float BLERadio::getLastRSSI() const { 
  // nRF52 doesn't provide easy access to RSSI during normal operation
  return -50; // Assume reasonable close-range RSSI
}

float BLERadio::getLastSNR() const { 
  return 10; // Assume good SNR for short-range BLE
}

int BLERadio::recvRaw(uint8_t* bytes, int sz) {
  int len = last_rx_len;
  if (last_rx_len > 0 && last_rx_len <= sz) {
    memcpy(bytes, rx_buf, last_rx_len);
    last_rx_len = 0;
    n_recv++;
    BLE_DEBUG_PRINTLN("Received packet, len=%d", len);
  } else if (last_rx_len > sz) {
    BLE_DEBUG_PRINTLN("Received packet too large (%d > %d), dropping", last_rx_len, sz);
    last_rx_len = 0;
    len = 0;
  }
  return len;
}

uint32_t BLERadio::getEstAirtimeFor(int len_bytes) {
  // BLE is relatively fast, but not as fast as ESP-NOW
  // BLE v4.0 max throughput is around 125-235 kbps practical
  // Estimate ~10ms for small packets
  return 10 + (len_bytes / 10); // Rough estimate
}

// Configuration methods
void BLERadio::setTargetMAC(const char* mac_address) {
  target_mac_address = String(mac_address);
  BLE_DEBUG_PRINTLN("Target MAC set to: %s", mac_address);
}

void BLERadio::setServiceUUID(const char* uuid) {
  // For nRF52, we use 16-bit UUIDs for simplicity
  // This could be extended to support 128-bit UUIDs
  service_uuid = strtoul(uuid, NULL, 16);
  BLE_DEBUG_PRINTLN("Service UUID set to: 0x%04X", service_uuid);
}

void BLERadio::setTxCharUUID(const char* uuid) {
  tx_char_uuid = strtoul(uuid, NULL, 16);
  BLE_DEBUG_PRINTLN("TX characteristic UUID set to: 0x%04X", tx_char_uuid);
}

void BLERadio::setRxCharUUID(const char* uuid) {
  rx_char_uuid = strtoul(uuid, NULL, 16);
  BLE_DEBUG_PRINTLN("RX characteristic UUID set to: 0x%04X", rx_char_uuid);
}

void BLERadio::setBLETxPower(uint8_t power) {
  setTxPower(power);
  BLE_DEBUG_PRINTLN("BLE TX power set to: %d", power);
}

void BLERadio::setAutoAdvertising(bool enable) {
  auto_advertising_enabled = enable;
  BLE_DEBUG_PRINTLN("Auto advertising %s", enable ? "enabled" : "disabled");
  
  if (enable && !Bluefruit.Advertising.isRunning()) {
    // Start advertising if it wasn't started before
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addService(bleService);
    Bluefruit.Advertising.addName();
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 244);
    Bluefruit.Advertising.setFastTimeout(30);
    Bluefruit.Advertising.start(0);
    BLE_DEBUG_PRINTLN("Advertising started");
  } else if (!enable && Bluefruit.Advertising.isRunning()) {
    // Stop advertising
    Bluefruit.Advertising.stop();
    BLE_DEBUG_PRINTLN("Advertising stopped");
  }
}

void BLERadio::connectToTarget() {
  if (target_mac_address.length() == 0) {
    BLE_DEBUG_PRINTLN("No target MAC address configured");
    return;
  }
  
  // Note: For nRF52 BLE, manual connection to a specific MAC requires 
  // implementing a BLE central role, which is complex. For now, we suggest
  // using advertising mode or implementing central functionality.
  BLE_DEBUG_PRINTLN("Manual connection to %s not yet implemented - use advertising mode", target_mac_address.c_str());
}

void BLERadio::disconnect() {
  if (deviceConnected) {
    // Disconnect current connection
    Bluefruit.disconnect(Bluefruit.connHandle());
    BLE_DEBUG_PRINTLN("BLE disconnected");
  }
}

bool BLERadio::isConnected() const {
  return deviceConnected;
}

void BLERadio::getStatus(char* status_buffer) {
  sprintf(status_buffer, "BLE: %s, Target: %s, Auto-adv: %s, UUIDs: 0x%04X/0x%04X/0x%04X", 
          deviceConnected ? "Connected" : "Disconnected",
          target_mac_address.length() > 0 ? target_mac_address.c_str() : "None",
          auto_advertising_enabled ? "On" : "Off",
          service_uuid,
          tx_char_uuid,
          rx_char_uuid);
}