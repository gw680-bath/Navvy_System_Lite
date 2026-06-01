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
  int driveLeftUs = 1500;
  int driveRightUs = 1500;
  int displaySteeringUs = 1500;
  int displayThrottleUs = 1500;
  int displayDriveLeftUs = 1500;
  int displayDriveRightUs = 1500;
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
  bool hasDrive = false;
  int driveLeftUs = 1500;
  int driveRightUs = 1500;
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
  bool webControlEngaged(uint32_t nowMs) const;
  void updateRcNeutralCalibration(const RcInputState &rcState, uint32_t nowMs) const;
  int clampUs(int pulseUs) const;

  AppConfig config_;
  ControlSource requestedSource_ = ControlSource::Rc;
  bool armed_ = false;
  int webSteeringUs_ = 1500;
  int webThrottleUs_ = 1500;
  ControlMode webMode_ = ControlMode::Joystick;
  bool webConnected_ = false;
  uint32_t lastWebUpdateMs_ = 0;
  uint32_t lastWebEngagedMs_ = 0;
  int webDriveLeftUs_ = 1500;
  int webDriveRightUs_ = 1500;
  mutable int rcNeutralCh1Us_ = 1500;
  mutable int rcNeutralCh2Us_ = 1500;
  mutable bool rcNeutralCaptured_ = false;
  mutable uint32_t rcCalibrationStartedMs_ = 0;
  mutable uint16_t rcCalibrationSamples_ = 0;
  mutable int32_t rcCalibrationSumCh1_ = 0;
  mutable int32_t rcCalibrationSumCh2_ = 0;
};

}  // namespace navvy
