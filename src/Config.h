#pragma once

#include <Arduino.h>
#include <Preferences.h>

namespace navvy {

enum class WifiMode : uint8_t {
  AccessPoint = 0,
  Station = 1,
};

struct AppConfig {
  uint8_t rcCh1Pin = 34;
  uint8_t rcCh2Pin = 35;
  uint8_t pwmSteeringPin = 25;
  uint8_t pwmThrottlePin = 26;
  uint8_t batterySensePin = 36;

  bool batterySenseEnabled = false;
  float batteryDividerRatio = 2.0f;
  float batteryReferenceVoltage = 3.3f;

  uint16_t rcFailsafeTimeoutMs = 120;
  uint16_t telemetryIntervalMs = 120;
  uint16_t webControlTimeoutMs = 500;

  int neutralUs = 1500;
  int minUs = 1000;
  int maxUs = 2000;
  int armingToleranceUs = 30;

  WifiMode wifiMode = WifiMode::AccessPoint;
  char apSsid[32] = "NavvyLite-ESP32";
  char apPassword[64] = "navvy1234";
  char staSsid[32] = "";
  char staPassword[64] = "";
};

class ConfigStore {
 public:
  bool begin(const char *namespaceName = "navvy");
  AppConfig load();
  bool save(const AppConfig &config);

 private:
  Preferences preferences_;
};

inline bool ConfigStore::begin(const char *namespaceName) {
  return preferences_.begin(namespaceName, false);
}

inline AppConfig ConfigStore::load() {
  AppConfig config;
  config.rcCh1Pin = preferences_.getUChar("rc1", config.rcCh1Pin);
  config.rcCh2Pin = preferences_.getUChar("rc2", config.rcCh2Pin);
  config.pwmSteeringPin = preferences_.getUChar("stOut", config.pwmSteeringPin);
  config.pwmThrottlePin = preferences_.getUChar("thOut", config.pwmThrottlePin);
  config.batterySensePin = preferences_.getUChar("batPin", config.batterySensePin);
  config.batterySenseEnabled = preferences_.getBool("batEn", config.batterySenseEnabled);
  config.batteryDividerRatio = preferences_.getFloat("batDiv", config.batteryDividerRatio);
  config.batteryReferenceVoltage = preferences_.getFloat("batRef", config.batteryReferenceVoltage);
  config.rcFailsafeTimeoutMs = preferences_.getUShort("rcTo", config.rcFailsafeTimeoutMs);
  config.telemetryIntervalMs = preferences_.getUShort("teleMs", config.telemetryIntervalMs);
  config.webControlTimeoutMs = preferences_.getUShort("webTo", config.webControlTimeoutMs);
  config.neutralUs = preferences_.getInt("neuUs", config.neutralUs);
  config.minUs = preferences_.getInt("minUs", config.minUs);
  config.maxUs = preferences_.getInt("maxUs", config.maxUs);
  config.armingToleranceUs = preferences_.getInt("armTol", config.armingToleranceUs);
  config.wifiMode = static_cast<WifiMode>(preferences_.getUChar("wifiMode", static_cast<uint8_t>(config.wifiMode)));
  preferences_.getBytes("apSsid", config.apSsid, sizeof(config.apSsid));
  preferences_.getBytes("apPass", config.apPassword, sizeof(config.apPassword));
  preferences_.getBytes("staSsid", config.staSsid, sizeof(config.staSsid));
  preferences_.getBytes("staPass", config.staPassword, sizeof(config.staPassword));
  return config;
}

inline bool ConfigStore::save(const AppConfig &config) {
  bool ok = true;
  ok &= preferences_.putUChar("rc1", config.rcCh1Pin) > 0;
  ok &= preferences_.putUChar("rc2", config.rcCh2Pin) > 0;
  ok &= preferences_.putUChar("stOut", config.pwmSteeringPin) > 0;
  ok &= preferences_.putUChar("thOut", config.pwmThrottlePin) > 0;
  ok &= preferences_.putUChar("batPin", config.batterySensePin) > 0;
  ok &= preferences_.putBool("batEn", config.batterySenseEnabled) > 0;
  ok &= preferences_.putFloat("batDiv", config.batteryDividerRatio) > 0;
  ok &= preferences_.putFloat("batRef", config.batteryReferenceVoltage) > 0;
  ok &= preferences_.putUShort("rcTo", config.rcFailsafeTimeoutMs) > 0;
  ok &= preferences_.putUShort("teleMs", config.telemetryIntervalMs) > 0;
  ok &= preferences_.putUShort("webTo", config.webControlTimeoutMs) > 0;
  ok &= preferences_.putInt("neuUs", config.neutralUs) > 0;
  ok &= preferences_.putInt("minUs", config.minUs) > 0;
  ok &= preferences_.putInt("maxUs", config.maxUs) > 0;
  ok &= preferences_.putInt("armTol", config.armingToleranceUs) > 0;
  ok &= preferences_.putUChar("wifiMode", static_cast<uint8_t>(config.wifiMode)) > 0;
  ok &= preferences_.putBytes("apSsid", config.apSsid, strnlen(config.apSsid, sizeof(config.apSsid)) + 1) > 0;
  ok &= preferences_.putBytes("apPass", config.apPassword, strnlen(config.apPassword, sizeof(config.apPassword)) + 1) > 0;
  ok &= preferences_.putBytes("staSsid", config.staSsid, strnlen(config.staSsid, sizeof(config.staSsid)) + 1) > 0;
  ok &= preferences_.putBytes("staPass", config.staPassword, strnlen(config.staPassword, sizeof(config.staPassword)) + 1) > 0;
  return ok;
}

}  // namespace navvy
