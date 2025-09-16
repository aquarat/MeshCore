#include <Arduino.h>
#include "target.h"
#include <helpers/ArduinoHelpers.h>

#ifdef BOARD_CLASS
  BOARD_CLASS board;
#endif

BLERadio radio_driver;

AutoDiscoverRTCClock rtc_clock;
EnvironmentSensorManager sensors;

bool radio_init() {
  rtc_clock.begin();

  radio_driver.init();

  return true;  // success
}

uint32_t radio_get_rng_seed() {
  return millis() + radio_driver.intID();
}

void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr) {
  // no-op for BLE
}

void radio_set_tx_power(uint8_t dbm) {
  radio_driver.setTxPower(dbm);
}

// nRF52 has hardware RNG
class nRF52_RNG : public mesh::RNG {
public:
  void random(uint8_t* dest, size_t sz) override {
    for (size_t i = 0; i < sz; i++) {
      dest[i] = (uint8_t)random(256);
    }
  }
};

mesh::LocalIdentity radio_new_identity() {
  nRF52_RNG rng;
  return mesh::LocalIdentity(&rng);  // create new random identity
}