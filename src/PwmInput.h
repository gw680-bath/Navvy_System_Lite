#pragma once

#include <Arduino.h>

#include "Config.h"

namespace navvy {

struct RcInputState {
  int ch1Us = 1500;
  int ch2Us = 1500;
  bool signalValid = false;
  bool available = false;
  uint32_t lastUpdateMs = 0;
};

class PwmInput {
 public:
  bool begin(const AppConfig &config);
  void update(uint32_t nowMs);

  int getCh1Us() const;
  int getCh2Us() const;
  bool rcAvailable(uint32_t nowMs) const;
  uint32_t lastUpdateMs() const;
  RcInputState snapshot(uint32_t nowMs) const;

 private:
  struct ChannelCapture {
    uint8_t pin = 0;
    volatile uint32_t riseMicros = 0;
    volatile uint16_t pulseUs = 1500;
    volatile bool high = false;
    volatile uint32_t lastEdgeMs = 0;
    volatile bool updated = false;
  };

  static void IRAM_ATTR handleEdge(void *arg);
  int sanitizePulse(uint16_t pulseUs) const;

  AppConfig config_;
  ChannelCapture ch1_;
  ChannelCapture ch2_;
  volatile uint32_t lastUpdateMs_ = 0;
  volatile bool available_ = false;
};

}  // namespace navvy
