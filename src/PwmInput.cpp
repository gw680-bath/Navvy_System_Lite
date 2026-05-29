#include "PwmInput.h"

namespace navvy {

namespace {
constexpr uint16_t kMinValidPulseUs = 750;
constexpr uint16_t kMaxValidPulseUs = 2250;
}

bool PwmInput::begin(const AppConfig &config) {
  config_ = config;

  ch1_.pin = config_.rcCh1Pin;
  ch2_.pin = config_.rcCh2Pin;

  pinMode(ch1_.pin, INPUT);
  pinMode(ch2_.pin, INPUT);

  attachInterruptArg(digitalPinToInterrupt(ch1_.pin), &PwmInput::handleEdge, &ch1_, CHANGE);
  attachInterruptArg(digitalPinToInterrupt(ch2_.pin), &PwmInput::handleEdge, &ch2_, CHANGE);

  lastUpdateMs_ = millis();
  available_ = false;
  return true;
}

void PwmInput::update(uint32_t nowMs) {
  if (ch1_.updated || ch2_.updated) {
    lastUpdateMs_ = nowMs;
    available_ = true;
    ch1_.updated = false;
    ch2_.updated = false;
  }

  if (nowMs - lastUpdateMs_ > config_.rcFailsafeTimeoutMs) {
    available_ = false;
  }
}

int PwmInput::sanitizePulse(uint16_t pulseUs) const {
  if (pulseUs < kMinValidPulseUs || pulseUs > kMaxValidPulseUs) {
    return config_.neutralUs;
  }
  return static_cast<int>(pulseUs);
}

int PwmInput::getCh1Us() const {
  return sanitizePulse(ch1_.pulseUs);
}

int PwmInput::getCh2Us() const {
  return sanitizePulse(ch2_.pulseUs);
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
    lastUpdateMs_ = channel->lastEdgeMs;
    available_ = true;
  }

  channel->high = false;
}

}  // namespace navvy
