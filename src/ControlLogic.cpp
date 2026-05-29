#include "ControlLogic.h"

namespace navvy {

void ControlLogic::begin(const AppConfig &config) {
  config_ = config;
  requestedSource_ = ControlSource::Rc;
  armed_ = false;
  webSteeringUs_ = config_.neutralUs;
  webThrottleUs_ = config_.neutralUs;
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
  if (requestedSource_ == ControlSource::Rc) {
    if (!rcState.signalValid || !isNeutral(rcState.ch2Us)) {
      armed_ = false;
      return false;
    }
  } else if (requestedSource_ == ControlSource::Web) {
    if (!webControlFresh(nowMs) || !isNeutral(webThrottleUs_)) {
      armed_ = false;
      return false;
    }
  } else {
    armed_ = false;
    return false;
  }

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

  if (!armed_) {
    output.currentSource = ControlSource::Failsafe;
    output.steeringUs = config_.neutralUs;
    output.throttleUs = config_.neutralUs;
    output.armed = false;
    return output;
  }

  if (requestedSource_ == ControlSource::Rc) {
    if (!rcState.signalValid) {
      output.currentSource = ControlSource::Failsafe;
      output.steeringUs = config_.neutralUs;
      output.throttleUs = config_.neutralUs;
      return output;
    }

    output.currentSource = ControlSource::Rc;
    output.steeringUs = clampUs(rcState.ch1Us);
    output.throttleUs = clampUs(rcState.ch2Us);
    output.armed = true;
    return output;
  }

  if (requestedSource_ == ControlSource::Web) {
    if (!webControlFresh(nowMs)) {
      output.currentSource = ControlSource::Failsafe;
      output.steeringUs = config_.neutralUs;
      output.throttleUs = config_.neutralUs;
      return output;
    }

    output.currentSource = ControlSource::Web;
    output.steeringUs = webSteeringUs_;
    output.throttleUs = webThrottleUs_;
    output.armed = true;
    return output;
  }

  output.currentSource = ControlSource::Failsafe;
  output.steeringUs = config_.neutralUs;
  output.throttleUs = config_.neutralUs;
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
