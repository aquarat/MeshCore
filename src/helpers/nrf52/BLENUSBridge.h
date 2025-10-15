#pragma once

#ifdef NRF52_PLATFORM

#include "helpers/bridges/BridgeBase.h"
#include "helpers/CommonCLI.h"
#include <bluefruit.h>
#include <Packet.h>   // for MAX_PACKET_PAYLOAD / MAX_PATH_SIZE

// Forward-declare Bluefruit Central UART client (provided by Adafruit nRF52 core)
class BLEClientUart;

/**
 * BLE NUS (Nordic UART Service) bridge for nrf52 backhaul
 *
 * - Uses Bluefruit BLEUart when acting as Peripheral/Server
 * - Uses Bluefruit BLEClientUart when acting as Central/Client
 * - Frames packets with magic + length + payload + checksum (same as RS232Bridge)
 */
class BLENUSBridge : public BridgeBase {
public:
  explicit BLENUSBridge(mesh::PacketManager* mgr, mesh::RTCClock* rtc, NodePrefs* prefs);

  // AbstractBridge
  void begin() override;
  void loop() override;
  void onPacketTransmitted(mesh::Packet* packet) override;
  void onPacketReceived(mesh::Packet* packet) override;

  // Re-read prefs and (re)initialize BLE as needed
  void reconfigure();

private:
  static BLENUSBridge* _instance;

  NodePrefs* _prefs;

  // Config cache (copied from prefs at begin/reconfigure)
  bool     _enabled;
  uint8_t  _role;       // 0=Peripheral, 1=Central
  int8_t   _tx_power;   // dBm
  uint8_t  _peer_mac[6];

  // Peripheral (server) UART
  BLEUart _periphUart;

  // Central (client) UART
  BLEClientUart* _clientUart; // allocated at runtime to reduce compile deps
  bool _centralReady;
  bool _centralConnected;

  // RX parser buffer/state (shared by both roles)
  static constexpr uint16_t SERIAL_OVERHEAD = BRIDGE_MAGIC_SIZE + BRIDGE_LENGTH_SIZE + BRIDGE_CHECKSUM_SIZE;

  // Max on-wire mesh packet length (header + transport + pathlen + path + payload)
  static constexpr uint16_t MAX_WIRE_LEN = 1 /*hdr*/ + 4 /*transport*/ + 1 /*path_len*/ + MAX_PATH_SIZE + MAX_PACKET_PAYLOAD;
  static_assert(MAX_WIRE_LEN <= 255, "wire length must fit in a byte");

  static constexpr uint16_t MAX_BLE_PACKET_SIZE = MAX_WIRE_LEN + SERIAL_OVERHEAD;

  uint8_t  _rx_buffer[MAX_BLE_PACKET_SIZE];
  uint16_t _rx_pos;

  // Internal helpers
  void cachePrefs();
  void stopAll();
  void initPeripheral();
  void initCentral();

  void centralStartScan();
  static void scanCallback(ble_gap_evt_adv_report_t* report);
  static void connectCallback(uint16_t conn_handle);
  static void disconnectCallback(uint16_t conn_handle, uint8_t reason);

  // Feed one byte into the RS232-like frame parser; when a full frame is assembled returns length, else 0
  uint16_t feedParser(uint8_t byte);
  void processParsedPacket(uint16_t len);

  // Common write helper (handles both roles)
  size_t writeBytes(const uint8_t* data, size_t len);
};

#endif // NRF52_PLATFORM