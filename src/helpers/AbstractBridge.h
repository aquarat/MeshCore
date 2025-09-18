#pragma once

#include <Mesh.h>

class AbstractBridge {
public:
  virtual ~AbstractBridge() {}

  // Initialize the bridge (set up radios/links, callbacks, etc)
  virtual void begin() = 0;

  // Called from main loop regularly to process IO
  virtual void loop() = 0;

  // Called after a mesh packet is transmitted over LoRa; bridge may forward it over backhaul
  virtual void onPacketTransmitted(mesh::Packet* packet) = 0;

  // Called by the bridge implementation when it receives a packet from the backhaul
  virtual void onPacketReceived(mesh::Packet* packet) = 0;
};