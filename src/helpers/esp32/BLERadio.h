#pragma once

#include <Mesh.h>

class BLERadio : public mesh::Radio {
protected:
  uint32_t n_recv, n_sent;

public:
  BLERadio() { n_recv = n_sent = 0; }

  void init();
  int recvRaw(uint8_t* bytes, int sz) override;
  uint32_t getEstAirtimeFor(int len_bytes) override;
  bool startSendRaw(const uint8_t* bytes, int len) override;
  bool isSendComplete() override;
  void onSendFinished() override;
  bool isInRecvMode() const override;

  uint32_t getPacketsRecv() const { return n_recv; }
  uint32_t getPacketsSent() const { return n_sent; }
  void resetStats() { n_recv = n_sent = 0; }

  virtual float getLastRSSI() const override;
  virtual float getLastSNR() const override;

  float packetScore(float snr, int packet_len) override { return 0; }
  uint32_t intID();
  void setTxPower(uint8_t dbm);
  
  // Configuration methods for manual pairing
  void setTargetMAC(const char* mac_address);
  void setServiceUUID(const char* uuid);
  void setTxCharUUID(const char* uuid);
  void setRxCharUUID(const char* uuid);
  void setBLETxPower(uint8_t power);
  void setAutoAdvertising(bool enable);
  void connectToTarget();
  void disconnect();
  bool isConnected() const;
  void getStatus(char* status_buffer);
  void getMACAddress(char* mac_buffer);
};

#if BLE_DEBUG_LOGGING && ARDUINO
  #include <Arduino.h>
  #define BLE_DEBUG_PRINT(F, ...) Serial.printf("BLE-Radio: " F, ##__VA_ARGS__)
  #define BLE_DEBUG_PRINTLN(F, ...) Serial.printf("BLE-Radio: " F "\n", ##__VA_ARGS__)
#else
  #define BLE_DEBUG_PRINT(...) {}
  #define BLE_DEBUG_PRINTLN(...) {}
#endif