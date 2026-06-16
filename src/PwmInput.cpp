#include "PwmInput.h"

namespace navvy {

namespace {
constexpr uint16_t kMinValidPulseUs = 800;
constexpr uint16_t kMaxValidPulseUs = 2400;
constexpr uint8_t kSmoothingPreviousWeight = 3;
constexpr uint8_t kSmoothingCurrentWeight = 1;
constexpr uint16_t kPulseFreshMs = 100;
constexpr uint16_t kNeutralWindowUs = 50;
constexpr uint16_t kMovementThresholdUs = 20;
constexpr uint16_t kNeutralMovementTimeoutMs = 200;
}

bool PwmInput::begin(const AppConfig &config) {
  config_ = config;

  ch1_.pin = config_.rcCh1Pin;
  ch2_.pin = config_.rcCh2Pin;

  pinMode(ch1_.pin, INPUT_PULLDOWN);
  pinMode(ch2_.pin, INPUT_PULLDOWN);

  attachInterruptArg(digitalPinToInterrupt(ch1_.pin), &PwmInput::handleEdge, &ch1_, CHANGE);
  attachInterruptArg(digitalPinToInterrupt(ch2_.pin), &PwmInput::handleEdge, &ch2_, CHANGE);

  lastUpdateMs_ = millis();
  available_ = false;
  ch1SmoothedUs_ = config_.neutralUs;
  ch2SmoothedUs_ = config_.neutralUs;
  lastMovementCh1Us_ = config_.neutralUs;
  lastMovementCh2Us_ = config_.neutralUs;
  ch1LastPulseMs_ = 0;
  ch2LastPulseMs_ = 0;
  lastMovementMs_ = 0;
  ch1Seen_ = false;
  ch2Seen_ = false;
  return true;
}

void PwmInput::update(uint32_t nowMs) {
  bool ch1Updated = false;
  bool ch2Updated = false;
  uint16_t ch1PulseUs = config_.neutralUs;
  uint16_t ch2PulseUs = config_.neutralUs;

  noInterrupts();
  if (ch1_.updated) {
    ch1PulseUs = ch1_.pulseUs;
    ch1_.updated = false;
    ch1Updated = true;
  }
  if (ch2_.updated) {
    ch2PulseUs = ch2_.pulseUs;
    ch2_.updated = false;
    ch2Updated = true;
  }
  interrupts();

  if (ch1Updated) {
    ch1SmoothedUs_ = smoothPulse(ch1SmoothedUs_, sanitizePulse(ch1PulseUs), ch1Seen_);
    ch1LastPulseMs_ = nowMs;
    ch1Seen_ = true;
  }
  if (ch2Updated) {
    ch2SmoothedUs_ = smoothPulse(ch2SmoothedUs_, sanitizePulse(ch2PulseUs), ch2Seen_);
    ch2LastPulseMs_ = nowMs;
    ch2Seen_ = true;
  }

  if (ch1Updated || ch2Updated) {
    lastUpdateMs_ = nowMs;
    if (inputsMoved(ch1SmoothedUs_, ch2SmoothedUs_)) {
      lastMovementMs_ = nowMs;
      lastMovementCh1Us_ = ch1SmoothedUs_;
      lastMovementCh2Us_ = ch2SmoothedUs_;
    }
  }

  const bool pulseFresh = ch1Seen_ && ch2Seen_
      && channelPulseFresh(ch1LastPulseMs_, nowMs)
      && channelPulseFresh(ch2LastPulseMs_, nowMs);
  const bool movementRecent = lastMovementMs_ != 0
      && (nowMs - lastMovementMs_ <= kNeutralMovementTimeoutMs);
  const bool nearNeutral = inputsNearNeutral();
  available_ = pulseFresh && (!nearNeutral || movementRecent);
}

int PwmInput::sanitizePulse(uint16_t pulseUs) const {
  if (pulseUs < kMinValidPulseUs || pulseUs > kMaxValidPulseUs) {
    return config_.neutralUs;
  }
  return static_cast<int>(pulseUs);
}

int PwmInput::smoothPulse(int previousUs, int currentUs, bool hadPrevious) const {
  if (!hadPrevious) {
    return currentUs;
  }

  const int totalWeight = kSmoothingPreviousWeight + kSmoothingCurrentWeight;
  return ((previousUs * kSmoothingPreviousWeight) + (currentUs * kSmoothingCurrentWeight)
      + (totalWeight / 2)) / totalWeight;
}

bool PwmInput::channelPulseFresh(uint32_t pulseMs, uint32_t nowMs) const {
  return pulseMs != 0 && (nowMs - pulseMs <= kPulseFreshMs);
}

bool PwmInput::inputsNearNeutral() const {
  return abs(ch1SmoothedUs_ - config_.neutralUs) <= kNeutralWindowUs
      && abs(ch2SmoothedUs_ - config_.neutralUs) <= kNeutralWindowUs;
}

bool PwmInput::inputsMoved(int ch1Us, int ch2Us) const {
  return abs(ch1Us - lastMovementCh1Us_) > kMovementThresholdUs
      || abs(ch2Us - lastMovementCh2Us_) > kMovementThresholdUs;
}

int PwmInput::getCh1Us() const {
  return ch1Seen_ ? ch1SmoothedUs_ : config_.neutralUs;
}

int PwmInput::getCh2Us() const {
  return ch2Seen_ ? ch2SmoothedUs_ : config_.neutralUs;
}

bool PwmInput::rcAvailable(uint32_t nowMs) const {
  return available_ && (nowMs - lastUpdateMs_ <= config_.rcFailsafeTimeoutMs);
}

uint32_t PwmInput::lastUpdateMs() const {
  return lastUpdateMs_;
}

RcInputState PwmInput::snapshot(uint32_t nowMs) const {
  RcInputState state;
  state.ch1Us = getCh1Us();
  state.ch2Us = getCh2Us();
  state.signalValid = rcAvailable(nowMs);
  state.available = state.signalValid;
  state.lastUpdateMs = lastUpdateMs();
  return state;
}

void IRAM_ATTR PwmInput::handleEdge(void *arg) {
  auto *channel = static_cast<ChannelCapture *>(arg);
  if (channel == nullptr) {
    return;
  }

  const bool levelHigh = digitalRead(channel->pin) != 0;
  const uint32_t nowMicros = micros();

  if (levelHigh) {
    channel->riseMicros = nowMicros;
    channel->high = true;
    return;
  }

  if (!channel->high) {
    return;
  }

  const uint32_t width = nowMicros - channel->riseMicros;
  if (width >= kMinValidPulseUs && width <= kMaxValidPulseUs) {
    channel->pulseUs = static_cast<uint16_t>(width);
    channel->lastEdgeMs = millis();
    channel->updated = true;
  }

  channel->high = false;
}

}  // namespace navvy
