#include "ControlLogic.h"

namespace navvy {

namespace {
constexpr int kOutputDeadzoneUs = 10;
constexpr int kEngagementThresholdUs = 10;
constexpr uint8_t kOutputSmoothingPreviousWeight = 3;
constexpr uint8_t kOutputSmoothingTargetWeight = 1;

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

DriveMix mixDriveFromControl(int steeringUs,
                             int throttleUs,
                             int steeringNeutralUs,
                             int throttleNeutralUs,
                             const AppConfig &config) {
  const float steerNorm = usToNorm(steeringUs, steeringNeutralUs, config);
  const float throttleNorm = usToNorm(throttleUs, throttleNeutralUs, config);
  const float leftNorm = constrain(throttleNorm + steerNorm, -1.0f, 1.0f);
  const float rightNorm = constrain(throttleNorm - steerNorm, -1.0f, 1.0f);
  DriveMix mix;
  mix.leftUs = normToUs(leftNorm, config.neutralUs, config);
  mix.rightUs = normToUs(rightNorm, config.neutralUs, config);
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
  rcNeutralCaptured_ = true;
  rcCalibrationComplete_ = true;
  webMode_ = ControlMode::Joystick;
  webConnected_ = false;
  lastWebUpdateMs_ = 0;
  lastWebEngagedMs_ = 0;
  rcCalibrationStartedMs_ = 0;
  rcCalibrationSamples_ = 0;
  rcCalibrationSumCh1_ = 0;
  rcCalibrationSumCh2_ = 0;
  smoothedDriveLeftUs_ = config_.neutralUs;
  smoothedDriveRightUs_ = config_.neutralUs;
}

bool ControlLogic::isNeutral(int pulseUs) const {
  return abs(pulseUs - config_.neutralUs) <= config_.armingToleranceUs;
}

int ControlLogic::clampUs(int pulseUs) const {
  if (pulseUs < config_.minUs) {
    return config_.minUs;
  }
  if (pulseUs > config_.maxUs) {
    return config_.maxUs;
  }
  return pulseUs;
}

int ControlLogic::smoothOutputUs(int previousUs, int targetUs) const {
  if (targetUs == config_.neutralUs) {
    return config_.neutralUs;
  }

  const int totalWeight = kOutputSmoothingPreviousWeight + kOutputSmoothingTargetWeight;
  return ((previousUs * kOutputSmoothingPreviousWeight) + (targetUs * kOutputSmoothingTargetWeight)
      + (totalWeight / 2)) / totalWeight;
}

bool ControlLogic::webControlFresh(uint32_t nowMs) const {
  return webConnected_ && (nowMs - lastWebUpdateMs_ <= config_.webControlTimeoutMs);
}

bool ControlLogic::webControlEngaged(uint32_t nowMs) const {
  return webConnected_ && (nowMs - lastWebEngagedMs_ <= config_.webControlTimeoutMs);
}

void ControlLogic::updateRcNeutralCalibration(const RcInputState &rcState, uint32_t nowMs) const {
  constexpr uint32_t kCalibrationWindowMs = 1500;
  constexpr uint16_t kCalibrationSamplesTarget = 48;
  constexpr int kCalibrationMaxDeviationUs = 220;

  if (rcCalibrationComplete_ || !rcState.signalValid) {
    return;
  }

  if (rcCalibrationStartedMs_ == 0) {
    rcCalibrationStartedMs_ = nowMs;
  }

  const bool insideWindow = (nowMs - rcCalibrationStartedMs_) <= kCalibrationWindowMs;
  const bool nearCenter = abs(rcState.ch1Us - config_.neutralUs) <= kCalibrationMaxDeviationUs
      && abs(rcState.ch2Us - config_.neutralUs) <= kCalibrationMaxDeviationUs;

  if (insideWindow && rcCalibrationSamples_ < kCalibrationSamplesTarget && nearCenter) {
    rcCalibrationSumCh1_ += rcState.ch1Us;
    rcCalibrationSumCh2_ += rcState.ch2Us;
    rcCalibrationSamples_++;
  }

  if (rcCalibrationSamples_ >= kCalibrationSamplesTarget || !insideWindow) {
    if (rcCalibrationSamples_ > 0) {
      rcNeutralCh1Us_ = static_cast<int>(rcCalibrationSumCh1_ / rcCalibrationSamples_);
      rcNeutralCh2Us_ = static_cast<int>(rcCalibrationSumCh2_ / rcCalibrationSamples_);
    } else {
      rcNeutralCh1Us_ = config_.neutralUs;
      rcNeutralCh2Us_ = config_.neutralUs;
    }
    rcNeutralCaptured_ = true;
    rcCalibrationComplete_ = true;
  } else if (!rcNeutralCaptured_) {
    rcNeutralCh1Us_ = config_.neutralUs;
    rcNeutralCh2Us_ = config_.neutralUs;
  }
}

void ControlLogic::applyWebCommand(const WebCommand &command, uint32_t nowMs) {
  if (command.hasSource) {
    requestedSource_ = command.source;
  }

  if (command.hasControl) {
    if (command.claimSource) {
      requestedSource_ = ControlSource::Web;
    }
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
      const DriveMix driveMix = mixDriveFromControl(
          webSteeringUs_,
          webThrottleUs_,
          config_.neutralUs,
          config_.neutralUs,
          config_);
      webDriveLeftUs_ = driveMix.leftUs;
      webDriveRightUs_ = driveMix.rightUs;
    }

    const bool engaged = command.hasDrive
      ? (abs(webDriveLeftUs_ - config_.neutralUs) > kEngagementThresholdUs
        || abs(webDriveRightUs_ - config_.neutralUs) > kEngagementThresholdUs)
      : (abs(webSteeringUs_ - config_.neutralUs) > kEngagementThresholdUs
        || abs(webThrottleUs_ - config_.neutralUs) > kEngagementThresholdUs);
    if (engaged) {
      lastWebEngagedMs_ = nowMs;
    }

    lastWebUpdateMs_ = nowMs;
  }

  if (command.connected) {
    webConnected_ = true;
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

  if (!rcCalibrationComplete_ && rcState.signalValid) {
    updateRcNeutralCalibration(rcState, nowMs);
  }

  const bool rcActive = rcState.signalValid && rcNeutralCaptured_;
  const bool webActive = webControlFresh(nowMs);

  if (webActive) {
    source = ControlSource::Web;
    sourceSteeringUs = webSteeringUs_;
    sourceThrottleUs = webThrottleUs_;
    sourceDriveLeftUs = webDriveLeftUs_;
    sourceDriveRightUs = webDriveRightUs_;
  } else if (rcActive) {
    source = ControlSource::Rc;
    sourceSteeringUs = clampUs(rcState.ch1Us);
    sourceThrottleUs = clampUs(rcState.ch2Us);
    const DriveMix driveMix = mixDriveFromControl(
        sourceSteeringUs,
        sourceThrottleUs,
        rcNeutralCh1Us_,
        rcNeutralCh2Us_,
        config_);
    sourceDriveLeftUs = driveMix.leftUs;
    sourceDriveRightUs = driveMix.rightUs;
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
    smoothedDriveLeftUs_ = config_.neutralUs;
    smoothedDriveRightUs_ = config_.neutralUs;
    output.driveLeftUs = 0;
    output.driveRightUs = 0;
    return output;
  }

  if (abs(sourceDriveLeftUs - config_.neutralUs) <= kOutputDeadzoneUs) {
    sourceDriveLeftUs = config_.neutralUs;
  }
  if (abs(sourceDriveRightUs - config_.neutralUs) <= kOutputDeadzoneUs) {
    sourceDriveRightUs = config_.neutralUs;
  }

  output.steeringUs = sourceSteeringUs;
  output.throttleUs = sourceThrottleUs;
  smoothedDriveLeftUs_ = smoothOutputUs(smoothedDriveLeftUs_, sourceDriveLeftUs);
  smoothedDriveRightUs_ = smoothOutputUs(smoothedDriveRightUs_, sourceDriveRightUs);
  output.driveLeftUs = sourceDriveLeftUs == config_.neutralUs ? 0 : smoothedDriveLeftUs_;
  output.driveRightUs = sourceDriveRightUs == config_.neutralUs ? 0 : smoothedDriveRightUs_;
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
