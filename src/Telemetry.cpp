#include "Telemetry.h"

#include <ArduinoJson.h>

namespace navvy {

void Telemetry::begin(const AppConfig &config) {
  config_ = config;
  lastSentMs_ = 0;
}

bool Telemetry::due(uint32_t nowMs) const {
  return nowMs - lastSentMs_ >= config_.telemetryIntervalMs;
}

void Telemetry::markSent(uint32_t nowMs) {
  lastSentMs_ = nowMs;
}

String Telemetry::buildJson(const ControlOutput &output,
                            const RcInputState &rcState,
                            float batteryVoltage,
                            uint32_t nowMs) const {
  StaticJsonDocument<384> doc;
  doc["ms"] = nowMs;
  doc["armed"] = output.armed;
  doc["source"] = static_cast<uint8_t>(output.currentSource);
  doc["rcOk"] = rcState.signalValid;
  doc["webOk"] = output.webConnected;
  doc["steeringUs"] = output.displaySteeringUs;
  doc["throttleUs"] = output.displayThrottleUs;
  doc["throttleLeftUs"] = output.displayDriveLeftUs;
  doc["throttleRightUs"] = output.displayDriveRightUs;
  doc["motorThrottleLeftUs"] = output.driveLeftUs;
  doc["motorThrottleRightUs"] = output.driveRightUs;
  doc["motorSteeringUs"] = output.steeringUs;
  doc["motorThrottleUs"] = output.throttleUs;
  if (batteryVoltage > 0.0f) {
    doc["batteryV"] = batteryVoltage;
  }
  doc["rcCh1Us"] = rcState.ch1Us;
  doc["rcCh2Us"] = rcState.ch2Us;

  String payload;
  serializeJson(doc, payload);
  return payload;
}

}  // namespace navvy
