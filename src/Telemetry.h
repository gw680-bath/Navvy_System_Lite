#pragma once

#include <Arduino.h>

#include "Config.h"
#include "ControlLogic.h"
#include "PwmInput.h"

namespace navvy {

class Telemetry {
 public:
  void begin(const AppConfig &config);
  bool due(uint32_t nowMs) const;
  void markSent(uint32_t nowMs);
  String buildJson(const ControlOutput &output,
                   const RcInputState &rcState,
                   float batteryVoltage,
                   uint32_t nowMs) const;

 private:
  AppConfig config_;
  uint32_t lastSentMs_ = 0;
};

}  // namespace navvy
