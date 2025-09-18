#pragma once

#include "helpers/AbstractBridge.h"
#include "helpers/SimpleMeshTables.h"

#include <RTClib.h>

/**
 * Base class implementing common bridge functionality
 *
 * Features:
 * - Fletcher-16 checksum calculation for data integrity
 * - Packet duplicate detection using SimpleMeshTables
 * - Common timestamp formatting for debug logging
 * - Shared packet management and queuing logic
 */
class BridgeBase : public AbstractBridge {
public:
  virtual ~BridgeBase() = default;

  // Magic to identify bridge frames on the wire
  static constexpr uint16_t BRIDGE_PACKET_MAGIC = 0xC03E;

  // Common field sizes
  static constexpr uint16_t BRIDGE_MAGIC_SIZE = sizeof(BRIDGE_PACKET_MAGIC);
  static constexpr uint16_t BRIDGE_LENGTH_SIZE = sizeof(uint16_t);
  static constexpr uint16_t BRIDGE_CHECKSUM_SIZE = sizeof(uint16_t);

  // Default delay (ms) for scheduling inbound packet processing
  static constexpr uint16_t BRIDGE_DELAY = 500;

protected:
  mesh::PacketManager* _mgr;
  mesh::RTCClock* _rtc;
  SimpleMeshTables _seen_packets;

  BridgeBase(mesh::PacketManager* mgr, mesh::RTCClock* rtc) : _mgr(mgr), _rtc(rtc) {}

  // For logging
  const char* getLogDateTime();

  // Checksum helpers
  static uint16_t fletcher16(const uint8_t *data, size_t len);
  bool validateChecksum(const uint8_t *data, size_t len, uint16_t received_checksum);

  // Standard handling for received packets (dup check + inbound queueing)
  void handleReceivedPacket(mesh::Packet *packet);
};