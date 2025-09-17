#pragma once

#include "Mesh.h"
#include <helpers/IdentityStore.h>

struct NodePrefs {  // persisted to file
    float airtime_factor;
    char node_name[32];
    double node_lat, node_lon;
    char password[16];
    float freq;
    uint8_t tx_power_dbm;
    uint8_t disable_fwd;
    uint8_t advert_interval;   // minutes / 2
    uint8_t flood_advert_interval;   // hours
    float rx_delay_base;
    float tx_delay_factor;
    char guest_password[16];
    float direct_tx_delay_factor;
    uint32_t guard;
    uint8_t sf;
    uint8_t cr;
    uint8_t allow_read_only;
    uint8_t multi_acks;
    float bw;
    uint8_t flood_max;
    uint8_t interference_threshold;
    uint8_t agc_reset_interval;   // secs / 4
    // BLE backhaul configuration
    char ble_target_mac[18];          // MAC address of target device (format: XX:XX:XX:XX:XX:XX)
    char ble_service_uuid[37];        // Service UUID (128-bit in string format)
    char ble_tx_char_uuid[37];        // TX characteristic UUID
    char ble_rx_char_uuid[37];        // RX characteristic UUID
    uint8_t ble_tx_power;             // BLE transmit power
    uint8_t ble_auto_advertising;     // 1 = enable auto advertising, 0 = manual pairing only
};

class CommonCLICallbacks {
public:
  virtual void savePrefs() = 0;
  virtual const char* getFirmwareVer() = 0;
  virtual const char* getBuildDate() = 0;
  virtual const char* getRole() = 0;
  virtual bool formatFileSystem() = 0;
  virtual void sendSelfAdvertisement(int delay_millis) = 0;
  virtual void updateAdvertTimer() = 0;
  virtual void updateFloodAdvertTimer() = 0;
  virtual void setLoggingOn(bool enable) = 0;
  virtual void eraseLogFile() = 0;
  virtual void dumpLogFile() = 0;
  virtual void setTxPower(uint8_t power_dbm) = 0;
  virtual void formatNeighborsReply(char *reply) = 0;
  virtual void removeNeighbor(const uint8_t* pubkey, int key_len) {
    // no op by default
  };
  virtual mesh::LocalIdentity& getSelfId() = 0;
  virtual void saveIdentity(const mesh::LocalIdentity& new_id) = 0;
  // BLE backhaul specific callbacks
  virtual void setBLETargetMAC(const char* mac_address) { /* no op by default */ }
  virtual void setBLEServiceUUID(const char* uuid) { /* no op by default */ }
  virtual void setBLETxCharUUID(const char* uuid) { /* no op by default */ }
  virtual void setBLERxCharUUID(const char* uuid) { /* no op by default */ }
  virtual void setBLETxPower(uint8_t power) { /* no op by default */ }
  virtual void setBLEAutoAdvertising(bool enable) { /* no op by default */ }
  virtual void connectBLETarget() { /* no op by default */ }
  virtual void disconnectBLE() { /* no op by default */ }
  virtual void getBLEStatus(char* reply) { strcpy(reply, "BLE not supported"); }
  virtual void getBLEMACAddress(char* reply) { strcpy(reply, "BLE not supported"); }
  virtual void clearStats() = 0;
  virtual void applyTempRadioParams(float freq, float bw, uint8_t sf, uint8_t cr, int timeout_mins) = 0;
};

class CommonCLI {
  mesh::RTCClock* _rtc;
  NodePrefs* _prefs;
  CommonCLICallbacks* _callbacks;
  mesh::MainBoard* _board;
  char tmp[PRV_KEY_SIZE*2 + 4];

  mesh::RTCClock* getRTCClock() { return _rtc; }
  void savePrefs();
  void loadPrefsInt(FILESYSTEM* _fs, const char* filename);

public:
  CommonCLI(mesh::MainBoard& board, mesh::RTCClock& rtc, NodePrefs* prefs, CommonCLICallbacks* callbacks)
      : _board(&board), _rtc(&rtc), _prefs(prefs), _callbacks(callbacks) { }

  void loadPrefs(FILESYSTEM* _fs);
  void savePrefs(FILESYSTEM* _fs);
  void handleCommand(uint32_t sender_timestamp, const char* command, char* reply);
};
