#include "ControlLogic.h"

namespace navvy {

namespace {
float usToNorm(int pulseUs, int neutralUs, const AppConfig &config) {
  const int positiveSpan = config.maxUs - neutralUs;
  const int negativeSpan = neutralUs - config.minUs;
  if (pulseUs >= neutralUs) {
    return positiveSpan > 0 ? static_cast<float>(pulseUs - neutralUs) / positiveSpan : 0.0f;
  }
  return negativeSpan > 0 ? static_cast<float>(pulseUs - neutralUs) / negativeSpan : 0.0f;
}

int normToUs(float value, int neutralUs, const AppConfig &config) {
  const float clamped = constrain(value, -1.0f, 1.0f);
  if (clamped >= 0.0f) {
    return static_cast<int>(lroundf(neutralUs + clamped * (config.maxUs - neutralUs)));
  }
  return static_cast<int>(lroundf(neutralUs + clamped * (neutralUs - config.minUs)));
}

struct DriveMix {
  int leftUs = 1500;
  int rightUs = 1500;
};

DriveMix mixDriveFromControl(int steeringUs, int throttleUs, int neutralUs, const AppConfig &config) {
  const float steerNorm = usToNorm(steeringUs, neutralUs, config);
  const float throttleNorm = usToNorm(throttleUs, neutralUs, config);
  const float leftNorm = constrain(throttleNorm + steerNorm, -1.0f, 1.0f);
  const float rightNorm = constrain(throttleNorm - steerNorm, -1.0f, 1.0f);
  DriveMix mix;
  mix.leftUs = normToUs(leftNorm, neutralUs, config);
  mix.rightUs = normToUs(rightNorm, neutralUs, config);
  return mix;
}

struct ControlMix {
  int steeringUs = 1500;
  int throttleUs = 1500;
};

ControlMix mixControlFromDrive(int leftUs, int rightUs, int neutralUs, const AppConfig &config) {
  const float leftNorm = usToNorm(leftUs, neutralUs, config);
  const float rightNorm = usToNorm(rightUs, neutralUs, config);
  const float steerNorm = constrain((rightNorm - leftNorm) * 0.5f, -1.0f, 1.0f);
  const float throttleNorm = constrain((leftNorm + rightNorm) * 0.5f, -1.0f, 1.0f);
  ControlMix mix;
  mix.steeringUs = normToUs(steerNorm, neutralUs, config);
  mix.throttleUs = normToUs(throttleNorm, neutralUs, config);
  return mix;
}
}  // namespace

void ControlLogic::begin(const AppConfig &config) {
  config_ = config;
  requestedSource_ = ControlSource::Rc;
  armed_ = false;
  webSteeringUs_ = config_.neutralUs;
  webThrottleUs_ = config_.neutralUs;
  webDriveLeftUs_ = config_.neutralUs;
  webDriveRightUs_ = config_.neutralUs;
  rcNeutralCh1Us_ = config_.neutralUs;
  rcNeutralCh2Us_ = config_.neutralUs;
  rcNeutralCaptured_ = false;
  webMode_ = ControlMode::Joystick;
  webConnected_ = false;
  lastWebUpdateMs_ = 0;
}

bool ControlLogic::isNeutral(int pulseUs) const {
  return abs(pulseUs - config_.neutralUs) <= config_.armingToleranceUs;
}

int ControlLogic::clampUs(int pulseUs) const {
  return constrain(pulseUs, config_.minUs, config_.maxUs);
}

bool ControlLogic::webControlFresh(uint32_t nowMs) const {
  return webConnected_ && (nowMs - lastWebUpdateMs_ <= config_.webControlTimeoutMs);
}

void ControlLogic::applyWebCommand(const WebCommand &command, uint32_t nowMs) {
  if (command.hasSource) {
    requestedSource_ = command.source;
  }

  if (command.hasControl) {
    webSteeringUs_ = clampUs(command.steeringUs);
    webThrottleUs_ = clampUs(command.throttleUs);
    webMode_ = command.mode;
    if (command.hasDrive) {
      webDriveLeftUs_ = clampUs(command.driveLeftUs);
      webDriveRightUs_ = clampUs(command.driveRightUs);
      const ControlMix controlMix = mixControlFromDrive(webDriveLeftUs_, webDriveRightUs_, config_.neutralUs, config_);
      webSteeringUs_ = controlMix.steeringUs;
      webThrottleUs_ = controlMix.throttleUs;
    } else {
      const DriveMix driveMix = mixDriveFromControl(webSteeringUs_, webThrottleUs_, config_.neutralUs, config_);
      webDriveLeftUs_ = driveMix.leftUs;
      webDriveRightUs_ = driveMix.rightUs;
    }
    lastWebUpdateMs_ = nowMs;
  }

  if (command.connected) {
    webConnected_ = true;
    lastWebUpdateMs_ = nowMs;
  } else if (command.disarmRequest) {
    webConnected_ = false;
  }

  if (command.disarmRequest) {
    disarm();
  }
}

bool ControlLogic::arm(const RcInputState &rcState, uint32_t nowMs) {
  armed_ = true;
  return true;
}

void ControlLogic::disarm() {
  armed_ = false;
}

ControlOutput ControlLogic::resolve(const RcInputState &rcState, uint32_t nowMs) const {
  ControlOutput output;
  output.rcSignalOk = rcState.signalValid;
  output.webConnected = webConnected_;

  int sourceSteeringUs = config_.neutralUs;
  int sourceThrottleUs = config_.neutralUs;
  int sourceDriveLeftUs = config_.neutralUs;
  int sourceDriveRightUs = config_.neutralUs;
  ControlSource source = ControlSource::Failsafe;

  if (requestedSource_ == ControlSource::Rc) {
    if (rcState.signalValid) {
      source = ControlSource::Rc;
      const_cast<ControlLogic *>(this)->rcNeutralCh1Us_ = rcNeutralCaptured_ ? rcNeutralCh1Us_ : rcState.ch1Us;
      const_cast<ControlLogic *>(this)->rcNeutralCh2Us_ = rcNeutralCaptured_ ? rcNeutralCh2Us_ : rcState.ch2Us;
      const_cast<ControlLogic *>(this)->rcNeutralCaptured_ = true;
      sourceSteeringUs = clampUs(rcState.ch1Us);
      sourceThrottleUs = clampUs(rcState.ch2Us);
      const DriveMix driveMix = mixDriveFromControl(sourceSteeringUs, sourceThrottleUs, rcNeutralCh1Us_, config_);
      sourceDriveLeftUs = driveMix.leftUs;
      sourceDriveRightUs = driveMix.rightUs;
    }
  } else if (requestedSource_ == ControlSource::Web) {
    if (!webControlFresh(nowMs)) {
      source = ControlSource::Failsafe;
    } else {
      source = ControlSource::Web;
      sourceSteeringUs = webSteeringUs_;
      sourceThrottleUs = webThrottleUs_;
      sourceDriveLeftUs = webDriveLeftUs_;
      sourceDriveRightUs = webDriveRightUs_;
    }
  }

  output.currentSource = source;
  output.displaySteeringUs = sourceSteeringUs;
  output.displayThrottleUs = sourceThrottleUs;
  output.displayDriveLeftUs = sourceDriveLeftUs;
  output.displayDriveRightUs = sourceDriveRightUs;
  output.armed = armed_;

  if (!armed_ || source == ControlSource::Failsafe) {
    output.steeringUs = config_.neutralUs;
    output.throttleUs = config_.neutralUs;
    output.driveLeftUs = config_.neutralUs;
    output.driveRightUs = config_.neutralUs;
    return output;
  }

  output.steeringUs = sourceSteeringUs;
  output.throttleUs = sourceThrottleUs;
  output.driveLeftUs = sourceDriveLeftUs;
  output.driveRightUs = sourceDriveRightUs;
  return output;
}

ControlSource ControlLogic::requestedSource() const {
  return requestedSource_;
}

bool ControlLogic::armed() const {
  return armed_;
}

int ControlLogic::webSteeringUs() const {
  return webSteeringUs_;
}

int ControlLogic::webThrottleUs() const {
  return webThrottleUs_;
}

ControlMode ControlLogic::webMode() const {
  return webMode_;
}

bool ControlLogic::webConnected() const {
  return webConnected_;
}

}  // namespace navvy
