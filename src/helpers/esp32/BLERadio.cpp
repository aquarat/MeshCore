#include "BLERadio.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_bt.h>

static BLEServer* pServer = NULL;
static BLECharacteristic* pTxCharacteristic = NULL;
static BLECharacteristic* pRxCharacteristic = NULL;
static bool deviceConnected = false;
static bool advertisingStarted = false;
static volatile bool is_send_complete = true;
static uint8_t rx_buf[256];
static uint8_t last_rx_len = 0;

// Configuration variables
static String target_mac_address = "";
static String service_uuid = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static String tx_char_uuid = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";
static String rx_char_uuid = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
static bool auto_advertising_enabled = false;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      BLE_DEBUG_PRINTLN("Device connected");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      BLE_DEBUG_PRINTLN("Device disconnected");
    }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      
      if (rxValue.length() > 0) {
        BLE_DEBUG_PRINTLN("Received %d bytes", rxValue.length());
        if (rxValue.length() <= sizeof(rx_buf)) {
          memcpy(rx_buf, rxValue.data(), rxValue.length());
          last_rx_len = rxValue.length();
        }
      }
    }
};

void BLERadio::init() {
  // Create the BLE Device
  BLEDevice::init("MeshCore-BLE-Backhaul");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(service_uuid.c_str());

  // Create a BLE Characteristic for transmission (TX from server perspective)
  pTxCharacteristic = pService->createCharacteristic(
                      tx_char_uuid.c_str(),
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pTxCharacteristic->addDescriptor(new BLE2902());

  // Create a BLE Characteristic for receiving (RX from server perspective)
  pRxCharacteristic = pService->createCharacteristic(
                      rx_char_uuid.c_str(),
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pRxCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  // Start the service
  pService->start();

  // Only start advertising if auto advertising is enabled
  if (auto_advertising_enabled) {
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(service_uuid.c_str());
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
    BLEDevice::startAdvertising();
    advertisingStarted = true;
    BLE_DEBUG_PRINTLN("Auto-advertising started, waiting for BLE backhaul connection...");
  } else {
    BLE_DEBUG_PRINTLN("BLE backhaul initialized - manual pairing mode");
    if (target_mac_address.length() > 0) {
      BLE_DEBUG_PRINTLN("Target MAC configured: %s", target_mac_address.c_str());
    }
  }
  
  is_send_complete = true;
}

void BLERadio::setTxPower(uint8_t dbm) {
  // ESP32 BLE power setting - convert to ESP_PWR_LVL enum values
  // Power levels: -12, -9, -6, -3, 0, 3, 6, 9 dBm
  esp_power_level_t power_level = ESP_PWR_LVL_P9;  // Default to max
  if (dbm <= -12) power_level = ESP_PWR_LVL_N12;
  else if (dbm <= -9) power_level = ESP_PWR_LVL_N9;
  else if (dbm <= -6) power_level = ESP_PWR_LVL_N6;
  else if (dbm <= -3) power_level = ESP_PWR_LVL_N3;
  else if (dbm <= 0) power_level = ESP_PWR_LVL_N0;
  else if (dbm <= 3) power_level = ESP_PWR_LVL_P3;
  else if (dbm <= 6) power_level = ESP_PWR_LVL_P6;
  
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, power_level);
}

uint32_t BLERadio::intID() {
  uint8_t mac[8];
  memset(mac, 0, sizeof(mac));
  esp_efuse_mac_get_default(mac);
  uint32_t n, m;
  memcpy(&n, &mac[0], 4);
  memcpy(&m, &mac[4], 4);
  
  return n + m;
}

bool BLERadio::startSendRaw(const uint8_t* bytes, int len) {
  if (!deviceConnected || !pTxCharacteristic) {
    BLE_DEBUG_PRINTLN("Send failed: not connected");
    return false;
  }

  // Send message via BLE notification
  is_send_complete = false;
  pTxCharacteristic->setValue((uint8_t*)bytes, len);
  pTxCharacteristic->notify();
  
  n_sent++;
  is_send_complete = true;  // BLE notifications are fire-and-forget
  BLE_DEBUG_PRINTLN("Send success, len=%d", len);
  return true;
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
  // BLE doesn't provide easy access to RSSI during normal operation
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
  service_uuid = String(uuid);
  BLE_DEBUG_PRINTLN("Service UUID set to: %s", uuid);
}

void BLERadio::setTxCharUUID(const char* uuid) {
  tx_char_uuid = String(uuid);
  BLE_DEBUG_PRINTLN("TX characteristic UUID set to: %s", uuid);
}

void BLERadio::setRxCharUUID(const char* uuid) {
  rx_char_uuid = String(uuid);
  BLE_DEBUG_PRINTLN("RX characteristic UUID set to: %s", uuid);
}

void BLERadio::setBLETxPower(uint8_t power) {
  setTxPower(power);
  BLE_DEBUG_PRINTLN("BLE TX power set to: %d", power);
}

void BLERadio::setAutoAdvertising(bool enable) {
  auto_advertising_enabled = enable;
  BLE_DEBUG_PRINTLN("Auto advertising %s", enable ? "enabled" : "disabled");
  
  if (enable && !advertisingStarted && pServer) {
    // Start advertising if it wasn't started before
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(service_uuid.c_str());
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0x0);
    BLEDevice::startAdvertising();
    advertisingStarted = true;
    BLE_DEBUG_PRINTLN("Advertising started");
  } else if (!enable && advertisingStarted) {
    // Stop advertising
    BLEDevice::stopAdvertising();
    advertisingStarted = false;
    BLE_DEBUG_PRINTLN("Advertising stopped");
  }
}

void BLERadio::connectToTarget() {
  if (target_mac_address.isEmpty()) {
    BLE_DEBUG_PRINTLN("No target MAC address configured");
    return;
  }
  
  // Note: For ESP32 BLE, manual connection to a specific MAC requires 
  // implementing a BLE client, which is complex. For now, we suggest
  // using advertising mode or implementing client functionality.
  BLE_DEBUG_PRINTLN("Manual connection to %s not yet implemented - use advertising mode", target_mac_address.c_str());
}

void BLERadio::disconnect() {
  if (deviceConnected && pServer) {
    // Disconnect all clients
    pServer->disconnect(pServer->getConnId());
    BLE_DEBUG_PRINTLN("BLE disconnected");
  }
}

bool BLERadio::isConnected() const {
  return deviceConnected;
}

void BLERadio::getStatus(char* status_buffer) {
  sprintf(status_buffer, "BLE: %s, Target: %s, Auto-adv: %s, UUIDs: %s/%s/%s",
          deviceConnected ? "Connected" : "Disconnected",
          target_mac_address.isEmpty() ? "None" : target_mac_address.c_str(),
          auto_advertising_enabled ? "On" : "Off",
          service_uuid.c_str(),
          tx_char_uuid.c_str(),
          rx_char_uuid.c_str());
}

void BLERadio::getMACAddress(char* mac_buffer) {
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  sprintf(mac_buffer, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}