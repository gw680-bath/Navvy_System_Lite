#pragma once

#include <Arduino.h>
#include <WebSocketsServer.h>
#include "C:/Users/Giles Woodland/.platformio/packages/framework-arduinoespressif32/libraries/WebServer/src/WebServer.h"

#include "Config.h"
#include "ControlLogic.h"

namespace navvy {

class NavvyWebServer {
 public:
  explicit NavvyWebServer(ControlLogic &controlLogic);

  bool begin(const AppConfig &config);
  void loop();

  void broadcastTelemetry(const String &jsonPayload);
  bool takeCommand(WebCommand &commandOut);
  bool clientConnected();

 private:
  static void onWebSocketEvent(uint8_t clientId, WStype_t type, uint8_t *payload, size_t length);
  void handleWebSocketEvent(uint8_t clientId, WStype_t type, uint8_t *payload, size_t length);
  void queueCommand(const WebCommand &command);

  WebServer httpServer_;
  WebSocketsServer wsServer_;
  ControlLogic &controlLogic_;
  AppConfig config_;
  bool hasPendingCommand_ = false;
  WebCommand pendingCommand_;
  static NavvyWebServer *instance_;
};

}  // namespace navvy
