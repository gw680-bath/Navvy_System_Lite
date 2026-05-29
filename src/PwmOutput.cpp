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
  if (pulseUs < config_.minUs || pulseUs > config_.maxUs) {
    return config_.neutralUs;
  }
  return pulseUs;
}

void PwmOutput::writeUs(int steeringUs, int throttleUs) {
  if (!started_) {
    return;
  }

  const uint32_t steeringDuty = usToDuty(static_cast<uint32_t>(clampUs(steeringUs)));
  const uint32_t throttleDuty = usToDuty(static_cast<uint32_t>(clampUs(throttleUs)));

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteChannel(kSteeringChannel, steeringDuty);
  ledcWriteChannel(kThrottleChannel, throttleDuty);
#else
  ledcWrite(kSteeringChannel, steeringDuty);
  ledcWrite(kThrottleChannel, throttleDuty);
#endif
}

void PwmOutput::neutral() {
  writeUs(config_.neutralUs, config_.neutralUs);
}

}  // namespace navvy
