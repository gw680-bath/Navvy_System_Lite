#pragma once

#include <Arduino.h>

#include "Config.h"

namespace navvy {

class PwmOutput {
 public:
  bool begin(const AppConfig &config);
  void writeUs(int steeringUs, int throttleUs);
  void neutral();

 private:
  int clampUs(int pulseUs) const;

  AppConfig config_;
  bool started_ = false;
};

}  // namespace navvy
