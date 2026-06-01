#include "PwmOutput.h"

namespace navvy {

namespace {
constexpr uint8_t kPwmResolutionBits = 16;
constexpr uint32_t kPwmFrequencyHz = 50;
constexpr uint32_t kServoPeriodUs = 20000;
constexpr uint8_t kSteeringChannel = 0;
constexpr uint8_t kThrottleChannel = 1;

uint32_t usToDuty(uint32_t pulseUs) {
  const uint32_t maxDuty = (1UL << kPwmResolutionBits) - 1;
  return (pulseUs * maxDuty) / kServoPeriodUs;
}
}  // namespace

bool PwmOutput::begin(const AppConfig &config) {
  config_ = config;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  started_ = ledcAttachChannel(config_.pwmSteeringPin, kPwmFrequencyHz, kPwmResolutionBits, kSteeringChannel);
  started_ &= ledcAttachChannel(config_.pwmThrottlePin, kPwmFrequencyHz, kPwmResolutionBits, kThrottleChannel);
#else
  ledcSetup(kSteeringChannel, kPwmFrequencyHz, kPwmResolutionBits);
  ledcSetup(kThrottleChannel, kPwmFrequencyHz, kPwmResolutionBits);
  ledcAttachPin(config_.pwmSteeringPin, kSteeringChannel);
  ledcAttachPin(config_.pwmThrottlePin, kThrottleChannel);
  started_ = true;
#endif

  neutral();
  return started_;
}

int PwmOutput::clampUs(int pulseUs) const {
  if (pulseUs == 0) {
    return 0;
  }
  if (pulseUs < config_.minUs || pulseUs > config_.maxUs) {
    return config_.neutralUs;
  }
  return pulseUs;
}

void PwmOutput::writeUs(int leftUs, int rightUs) {
  if (!started_) {
    return;
  }

  const uint32_t leftDuty = usToDuty(static_cast<uint32_t>(clampUs(leftUs)));
  const uint32_t rightDuty = usToDuty(static_cast<uint32_t>(clampUs(rightUs)));

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteChannel(kSteeringChannel, leftDuty);
  ledcWriteChannel(kThrottleChannel, rightDuty);
#else
  ledcWrite(kSteeringChannel, leftDuty);
  ledcWrite(kThrottleChannel, rightDuty);
#endif
}

void PwmOutput::neutral() {
  writeUs(0, 0);
}

}  // namespace navvy
