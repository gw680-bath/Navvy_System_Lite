#include <Arduino.h>

#include "Config.h"
#include "ControlLogic.h"
#include "PwmInput.h"
#include "PwmOutput.h"
#include "Telemetry.h"
#include "WebServer.h"

using namespace navvy;

namespace {
ConfigStore configStore;
AppConfig appConfig;
PwmInput pwmInput;
PwmOutput pwmOutput;
ControlLogic controlLogic;
NavvyWebServer webServer(controlLogic);
Telemetry telemetry;

float readBatteryVoltage(const AppConfig &config) {
  if (!config.batterySenseEnabled) {
    return 0.0f;
  }

  analogReadResolution(12);
  analogSetPinAttenuation(config.batterySensePin, ADC_11db);
  const uint32_t raw = analogRead(config.batterySensePin);
  const float measuredVoltage = (static_cast<float>(raw) / 4095.0f) * config.batteryReferenceVoltage;
  return measuredVoltage * config.batteryDividerRatio;
}

void applyPendingCommand(const WebCommand &command) {
  const uint32_t nowMs = millis();

  controlLogic.applyWebCommand(command, nowMs);

  if (command.armRequest) {
    const RcInputState rcState = pwmInput.snapshot(nowMs);
    controlLogic.arm(rcState, nowMs);
  }

  if (command.disarmRequest) {
    controlLogic.disarm();
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  if (!configStore.begin()) {
    Serial.println("Config NVS could not be opened, using defaults.");
  }

  appConfig = configStore.load();

  controlLogic.begin(appConfig);
  telemetry.begin(appConfig);

  pwmInput.begin(appConfig);
  pwmOutput.begin(appConfig);
  webServer.begin(appConfig);

  Serial.println();
  Serial.println("Navvy Lite started.");
  Serial.print("AP SSID: ");
  Serial.println(appConfig.apSsid);
}

void loop() {
  const uint32_t nowMs = millis();
  pwmInput.update(nowMs);
  webServer.loop();

  WebCommand command;
  while (webServer.takeCommand(command)) {
    applyPendingCommand(command);
  }

  const RcInputState rcState = pwmInput.snapshot(nowMs);
  const ControlOutput output = controlLogic.resolve(rcState, nowMs);
  pwmOutput.writeUs(output.driveLeftUs, output.driveRightUs);

  if (telemetry.due(nowMs)) {
    const float batteryVoltage = readBatteryVoltage(appConfig);
    const String payload = telemetry.buildJson(output, rcState, batteryVoltage, nowMs);
    webServer.broadcastTelemetry(payload);
    telemetry.markSent(nowMs);
  }
}
