#pragma once

#include <Arduino.h>

#include "Config.h"
#include "PwmInput.h"

namespace navvy {

enum class ControlSource : uint8_t {
  Failsafe = 0,
  Rc = 1,
  Web = 2,
};

enum class ControlMode : uint8_t {
  Joystick = 0,
  Tank = 1,
};

struct ControlOutput {
  int steeringUs = 1500;
  int throttleUs = 1500;
  ControlSource currentSource = ControlSource::Failsafe;
  bool armed = false;
  bool rcSignalOk = false;
  bool webConnected = false;
};

struct WebCommand {
  bool hasSource = false;
  ControlSource source = ControlSource::Web;

  bool armRequest = false;
  bool disarmRequest = false;

  bool hasControl = false;
  int steeringUs = 1500;
  int throttleUs = 1500;
  ControlMode mode = ControlMode::Joystick;
  bool connected = false;
};

class ControlLogic {
 public:
  void begin(const AppConfig &config);
  void applyWebCommand(const WebCommand &command, uint32_t nowMs);
  bool arm(const RcInputState &rcState, uint32_t nowMs);
  void disarm();
  ControlOutput resolve(const RcInputState &rcState, uint32_t nowMs) const;

  ControlSource requestedSource() const;
  bool armed() const;
  int webSteeringUs() const;
  int webThrottleUs() const;
  ControlMode webMode() const;
  bool webConnected() const;

 private:
  bool isNeutral(int pulseUs) const;
  bool webControlFresh(uint32_t nowMs) const;
  int clampUs(int pulseUs) const;

  AppConfig config_;
  ControlSource requestedSource_ = ControlSource::Rc;
  bool armed_ = false;
  int webSteeringUs_ = 1500;
  int webThrottleUs_ = 1500;
  ControlMode webMode_ = ControlMode::Joystick;
  bool webConnected_ = false;
  uint32_t lastWebUpdateMs_ = 0;
};

}  // namespace navvy
