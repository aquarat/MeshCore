#include "BLERadio.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_bt.h>

#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static BLEServer* pServer = NULL;
static BLECharacteristic* pTxCharacteristic = NULL;
static BLECharacteristic* pRxCharacteristic = NULL;
static bool deviceConnected = false;
static bool advertisingStarted = false;
static volatile bool is_send_complete = true;
static uint8_t rx_buf[256];
static uint8_t last_rx_len = 0;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      BLE_DEBUG_PRINTLN("Device connected");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      BLE_DEBUG_PRINTLN("Device disconnected, restarting advertising");
      BLEDevice::startAdvertising();
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
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic for transmission (TX from server perspective)
  pTxCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pTxCharacteristic->addDescriptor(new BLE2902());

  // Create a BLE Characteristic for receiving (RX from server perspective)
  pRxCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_RX,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pRxCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  advertisingStarted = true;
  
  BLE_DEBUG_PRINTLN("Waiting for BLE backhaul connection...");
  
  is_send_complete = true;
}

void BLERadio::setTxPower(uint8_t dbm) {
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, (esp_power_level_t)dbm);
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