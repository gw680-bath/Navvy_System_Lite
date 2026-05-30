#include "WebServer.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <cstring>

namespace navvy {

NavvyWebServer *NavvyWebServer::instance_ = nullptr;

namespace {
const char kHtmlPage[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Navvy Lite</title>
  <style>
    :root {
      --bg: #0b1020;
      --panel: rgba(16, 22, 43, 0.88);
      --text: #eef2ff;
      --muted: #98a2c3;
      --accent: #4de1c1;
      --accent-2: #f7b801;
      --danger: #ff6b6b;
      --line: rgba(255,255,255,0.10);
      --shadow: 0 20px 60px rgba(0,0,0,0.35);
    }

    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      background:
        radial-gradient(circle at top left, rgba(77,225,193,0.20), transparent 30%),
        radial-gradient(circle at top right, rgba(247,184,1,0.18), transparent 28%),
        linear-gradient(160deg, #090d18 0%, #10162a 52%, #0b1020 100%);
      color: var(--text);
      min-height: 100vh;
      padding: 20px;
    }

    .shell { max-width: 1100px; margin: 0 auto; display: grid; gap: 18px; }
    .hero, .panel {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 24px;
      box-shadow: var(--shadow);
      backdrop-filter: blur(18px);
    }

    .hero {
      padding: 22px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 16px;
    }

    .brand h1 { margin: 0; font-size: clamp(28px, 4vw, 44px); letter-spacing: -0.04em; }
    .brand p { margin: 6px 0 0; color: var(--muted); max-width: 62ch; }
    .status-pill {
      padding: 10px 14px;
      border-radius: 999px;
      border: 1px solid var(--line);
      color: var(--muted);
      background: rgba(255,255,255,0.04);
      white-space: nowrap;
    }

    .grid { display: grid; grid-template-columns: 1.15fr 0.85fr; gap: 18px; }
    .panel { padding: 18px; }
    .controls-row, .telemetry-grid, .mode-row { display: grid; gap: 12px; }
    .controls-row { grid-template-columns: repeat(3, minmax(0, 1fr)); }
    .mode-row { grid-template-columns: repeat(2, minmax(0, 1fr)); }
    .telemetry-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); }

    .button {
      border: 1px solid var(--line);
      background: linear-gradient(180deg, rgba(255,255,255,0.08), rgba(255,255,255,0.03));
      color: var(--text);
      border-radius: 16px;
      padding: 14px 16px;
      font-size: 15px;
      font-weight: 700;
      cursor: pointer;
      transition: transform 120ms ease, border-color 120ms ease, background 120ms ease;
    }

    .button:hover { transform: translateY(-1px); border-color: rgba(77,225,193,0.45); }
    .button.primary { background: linear-gradient(135deg, rgba(77,225,193,0.28), rgba(77,225,193,0.12)); }
    .button.warn { background: linear-gradient(135deg, rgba(247,184,1,0.28), rgba(247,184,1,0.12)); }
    .button.danger { background: linear-gradient(135deg, rgba(255,107,107,0.28), rgba(255,107,107,0.12)); }
    .button.active { border-color: rgba(77,225,193,0.75); box-shadow: inset 0 0 0 1px rgba(77,225,193,0.25); }

    .card {
      background: rgba(255,255,255,0.04);
      border: 1px solid var(--line);
      border-radius: 18px;
      padding: 14px;
    }

    .card h3, .card h4 {
      margin: 0 0 8px;
      font-size: 14px;
      color: var(--muted);
      text-transform: uppercase;
      letter-spacing: 0.08em;
    }

    .value { font-size: 34px; font-weight: 800; letter-spacing: -0.05em; }
    .subvalue { color: var(--muted); margin-top: 4px; }
    .joystick-wrap { display: grid; gap: 12px; }

    .joystick {
      position: relative;
      width: min(100%, 420px);
      aspect-ratio: 1;
      margin: 0 auto;
      border-radius: 28px;
      border: 1px solid var(--line);
      background:
        radial-gradient(circle at center, rgba(77,225,193,0.08), transparent 55%),
        linear-gradient(180deg, rgba(255,255,255,0.06), rgba(255,255,255,0.02));
      overflow: hidden;
      touch-action: none;
    }

    .joystick::before, .joystick::after {
      content: "";
      position: absolute;
      left: 50%; top: 50%;
      background: rgba(255,255,255,0.10);
      transform: translate(-50%, -50%);
    }

    .joystick::before { width: 1px; height: 100%; }
    .joystick::after { width: 100%; height: 1px; }

    .knob {
      position: absolute;
      left: 50%; top: 50%;
      width: 84px; height: 84px;
      border-radius: 50%;
      transform: translate(-50%, -50%);
      background: radial-gradient(circle at 35% 35%, #ffffff, #8ff5dc 25%, #1e7d71 100%);
      box-shadow: 0 16px 40px rgba(0,0,0,0.32);
      border: 1px solid rgba(255,255,255,0.25);
    }

    .sliders { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 14px; }
    .slider-card input[type=range] { width: 100%; accent-color: var(--accent); }
    .slider-card .readout {
      display: flex;
      justify-content: space-between;
      color: var(--muted);
      font-size: 14px;
      margin-top: 8px;
    }

    .hidden { display: none !important; }
    .telemetry-grid .card .value { font-size: 24px; }
    .footer-note { color: var(--muted); font-size: 13px; text-align: center; padding-bottom: 8px; }

    @media (max-width: 860px) { .grid { grid-template-columns: 1fr; } }
    @media (max-width: 640px) {
      .controls-row, .mode-row, .telemetry-grid, .sliders { grid-template-columns: 1fr; }
      .hero { flex-direction: column; align-items: flex-start; }
    }
  </style>
</head>
<body>
  <div class="shell">
    <section class="hero">
      <div class="brand">
        <h1>Navvy Lite</h1>
        <p>ESP32 bridge for RC receiver input, Roboclaw output, and web control with safety-first arming and live telemetry.</p>
      </div>
      <div class="status-pill" id="connectionPill">Connecting...</div>
    </section>

    <section class="panel">
      <div class="controls-row">
        <button class="button primary" id="armBtn">ARM</button>
        <button class="button danger" id="disarmBtn">DISARM</button>
        <button class="button warn" id="sourceBtn">MODE: RC</button>
      </div>
      <div class="mode-row" style="margin-top: 12px;">
        <button class="button active" id="joystickModeBtn">Joystick</button>
        <button class="button" id="tankModeBtn">Tank</button>
      </div>
    </section>

    <div class="grid">
      <section class="panel">
        <div class="joystick-wrap">
          <div class="joystick hidden" id="joystickPad">
            <div class="knob" id="joystickKnob"></div>
          </div>

          <div class="sliders hidden" id="tankSliders">
            <div class="card slider-card">
              <h4>Left</h4>
              <input type="range" min="1000" max="2000" value="1500" id="leftSlider" />
              <div class="readout"><span>Neutral</span><span id="leftValue">1500</span></div>
            </div>
            <div class="card slider-card">
              <h4>Right</h4>
              <input type="range" min="1000" max="2000" value="1500" id="rightSlider" />
              <div class="readout"><span>Neutral</span><span id="rightValue">1500</span></div>
            </div>
          </div>
        </div>
      </section>

      <section class="panel">
        <div class="telemetry-grid">
          <div class="card"><h3>Armed</h3><div class="value" id="armedValue">NO</div><div class="subvalue" id="armedSub">Failsafe neutral</div></div>
          <div class="card"><h3>Source</h3><div class="value" id="sourceValue">RC</div><div class="subvalue" id="sourceSub">Input routing</div></div>
          <div class="card"><h3>Steering</h3><div class="value" id="steeringValue">1500</div><div class="subvalue">PWM microseconds</div></div>
          <div class="card"><h3>Throttle</h3><div class="value" id="throttleValue">1500</div><div class="subvalue">PWM microseconds</div></div>
          <div class="card"><h3>RC</h3><div class="value" id="rcValue">OK</div><div class="subvalue" id="rcSub">Signal health</div></div>
          <div class="card"><h3>Battery</h3><div class="value" id="batteryValue">--</div><div class="subvalue">Optional sense input</div></div>
        </div>
      </section>
    </div>

    <div class="footer-note">Web UI runs directly on the ESP32. If Wi-Fi disconnects, RC failsafe logic remains active.</div>
  </div>

  <script>
    const state = {
      mode: 'joystick',
      source: 'rc',
      armed: false,
      steerUs: 1500,
      throttleUs: 1500,
      socket: null,
      activePointer: false,
    };

    const els = {
      connectionPill: document.getElementById('connectionPill'),
      armBtn: document.getElementById('armBtn'),
      disarmBtn: document.getElementById('disarmBtn'),
      sourceBtn: document.getElementById('sourceBtn'),
      joystickModeBtn: document.getElementById('joystickModeBtn'),
      tankModeBtn: document.getElementById('tankModeBtn'),
      joystickPad: document.getElementById('joystickPad'),
      joystickKnob: document.getElementById('joystickKnob'),
      tankSliders: document.getElementById('tankSliders'),
      leftSlider: document.getElementById('leftSlider'),
      rightSlider: document.getElementById('rightSlider'),
      leftValue: document.getElementById('leftValue'),
      rightValue: document.getElementById('rightValue'),
      armedValue: document.getElementById('armedValue'),
      armedSub: document.getElementById('armedSub'),
      sourceValue: document.getElementById('sourceValue'),
      sourceSub: document.getElementById('sourceSub'),
      steeringValue: document.getElementById('steeringValue'),
      throttleValue: document.getElementById('throttleValue'),
      rcValue: document.getElementById('rcValue'),
      rcSub: document.getElementById('rcSub'),
      batteryValue: document.getElementById('batteryValue'),
    };

    function clamp(v, min, max) { return Math.max(min, Math.min(max, v)); }
    function usFromNorm(n) { return Math.round(1500 + clamp(n, -1, 1) * 500); }
    function normFromUs(us) { return clamp((us - 1500) / 500, -1, 1); }

    function updateButtons() {
      els.armBtn.classList.toggle('active', state.armed);
      els.sourceBtn.textContent = `MODE: ${state.source.toUpperCase()}`;
      els.sourceBtn.classList.toggle('active', state.source === 'web');
      els.joystickModeBtn.classList.toggle('active', state.mode === 'joystick');
      els.tankModeBtn.classList.toggle('active', state.mode === 'tank');
      els.joystickPad.classList.toggle('hidden', state.mode !== 'joystick');
      els.tankSliders.classList.toggle('hidden', state.mode !== 'tank');
    }

    function updateTelemetryView(data) {
      els.armedValue.textContent = data.armed ? 'YES' : 'NO';
      els.armedSub.textContent = data.armed ? 'Control active' : 'Neutral / disarmed';
      els.sourceValue.textContent = data.source === 1 ? 'RC' : data.source === 2 ? 'WEB' : 'SAFE';
      els.sourceSub.textContent = data.source === 0 ? 'Failsafe' : 'Routing source';
      els.steeringValue.textContent = data.steeringUs ?? 1500;
      els.throttleValue.textContent = data.throttleUs ?? 1500;
      els.rcValue.textContent = data.rcOk ? 'OK' : 'NO';
      els.rcSub.textContent = data.rcOk ? 'Receiver signal healthy' : 'Signal lost / stale';
      els.batteryValue.textContent = data.batteryV ? `${data.batteryV.toFixed(2)} V` : '--';
      state.armed = !!data.armed;
      state.source = data.source === 2 ? 'web' : 'rc';
      state.steerUs = data.steeringUs ?? 1500;
      state.throttleUs = data.throttleUs ?? 1500;
      updateButtons();
    }

    function send(obj) {
      if (!state.socket || state.socket.readyState !== WebSocket.OPEN) return;
      state.socket.send(JSON.stringify(obj));
    }

    function sendArm(armed) { send({ type: armed ? 'arm' : 'disarm' }); }
    function sendSource(source) { send({ type: 'source', source }); }
    function sendControl() { send({ type: 'control', mode: state.mode, steeringUs: state.steerUs, throttleUs: state.throttleUs }); }

    function setJoystickFromPointer(event) {
      const rect = els.joystickPad.getBoundingClientRect();
      const x = clamp((event.clientX - rect.left) / rect.width, 0, 1);
      const y = clamp((event.clientY - rect.top) / rect.height, 0, 1);
      const nx = (x - 0.5) * 2;
      const ny = (0.5 - y) * 2;
      const steer = usFromNorm(nx);
      const throttle = usFromNorm(ny);
      const knobX = clamp((x * 100), 8, 92);
      const knobY = clamp((y * 100), 8, 92);
      els.joystickKnob.style.left = `${knobX}%`;
      els.joystickKnob.style.top = `${knobY}%`;
      state.steerUs = steer;
      state.throttleUs = throttle;
      sendControl();
    }

    function resetJoystick() {
      els.joystickKnob.style.left = '50%';
      els.joystickKnob.style.top = '50%';
      state.steerUs = 1500;
      state.throttleUs = 1500;
      sendControl();
    }

    function setTankFromSliders() {
      const left = Number(els.leftSlider.value);
      const right = Number(els.rightSlider.value);
      els.leftValue.textContent = left;
      els.rightValue.textContent = right;
      const leftNorm = normFromUs(left);
      const rightNorm = normFromUs(right);
      const steerNorm = clamp((rightNorm - leftNorm) / 2, -1, 1);
      const throttleNorm = clamp((leftNorm + rightNorm) / 2, -1, 1);
      state.steerUs = usFromNorm(steerNorm);
      state.throttleUs = usFromNorm(throttleNorm);
      sendControl();
    }

    els.armBtn.addEventListener('click', () => sendArm(true));
    els.disarmBtn.addEventListener('click', () => sendArm(false));
    els.sourceBtn.addEventListener('click', () => {
      state.source = state.source === 'rc' ? 'web' : 'rc';
      updateButtons();
      sendSource(state.source);
    });
    els.joystickModeBtn.addEventListener('click', () => {
      state.mode = 'joystick';
      updateButtons();
      resetJoystick();
    });
    els.tankModeBtn.addEventListener('click', () => {
      state.mode = 'tank';
      updateButtons();
      setTankFromSliders();
    });

    els.leftSlider.addEventListener('input', setTankFromSliders);
    els.rightSlider.addEventListener('input', setTankFromSliders);

    els.joystickPad.addEventListener('pointerdown', (event) => {
      state.activePointer = true;
      els.joystickPad.setPointerCapture(event.pointerId);
      setJoystickFromPointer(event);
    });
    els.joystickPad.addEventListener('pointermove', (event) => {
      if (!state.activePointer) return;
      setJoystickFromPointer(event);
    });
    els.joystickPad.addEventListener('pointerup', () => {
      state.activePointer = false;
      resetJoystick();
    });
    els.joystickPad.addEventListener('pointercancel', () => {
      state.activePointer = false;
      resetJoystick();
    });

    function connect() {
      const wsUrl = `ws://${location.hostname}:81/`;
      const socket = new WebSocket(wsUrl);
      state.socket = socket;

      socket.onopen = () => {
        els.connectionPill.textContent = 'Connected';
        sendSource(state.source);
        sendControl();
      };

      socket.onclose = () => {
        els.connectionPill.textContent = 'Disconnected - retrying';
        setTimeout(connect, 1200);
      };

      socket.onerror = () => {
        els.connectionPill.textContent = 'WebSocket error';
      };

      socket.onmessage = (event) => {
        try { updateTelemetryView(JSON.parse(event.data)); } catch (_) {}
      };
    }

    updateButtons();
    resetJoystick();
    setTankFromSliders();
    connect();
  </script>
</body>
</html>
)HTML";

bool loadLeftHanded() {
  Preferences prefs;
  if (!prefs.begin("navvy", true)) {
    return false;
  }

  const bool leftHanded = prefs.getBool("uiLeftHanded", false);
  prefs.end();
  return leftHanded;
}

bool saveLeftHanded(bool leftHanded) {
  Preferences prefs;
  if (!prefs.begin("navvy", false)) {
    return false;
  }

  const bool ok = prefs.putBool("uiLeftHanded", leftHanded) > 0;
  prefs.end();
  return ok;
}

bool saveWifiSettings(const String &ssid, const String &password) {
  Preferences prefs;
  if (!prefs.begin("navvy", false)) {
    return false;
  }

  char ssidBuffer[32] = {0};
  char passwordBuffer[64] = {0};
  ssid.toCharArray(ssidBuffer, sizeof(ssidBuffer));
  password.toCharArray(passwordBuffer, sizeof(passwordBuffer));

  bool ok = true;
  ok &= prefs.putBytes("staSsid", ssidBuffer, strnlen(ssidBuffer, sizeof(ssidBuffer)) + 1) > 0;
  ok &= prefs.putBytes("staPass", passwordBuffer, strnlen(passwordBuffer, sizeof(passwordBuffer)) + 1) > 0;
  ok &= prefs.putUChar("wifiMode", static_cast<uint8_t>(WifiMode::Station)) > 0;
  prefs.end();
  return ok;
}

String activeNetworkName(const AppConfig &config) {
  const bool staConnected = WiFi.status() == WL_CONNECTED;
  if (config.wifiMode == WifiMode::Station && staConnected && WiFi.SSID().length() > 0) {
    return WiFi.SSID();
  }
  return String(config.apSsid);
}

String activeIpAddress(const AppConfig &config) {
  if (config.wifiMode == WifiMode::Station && WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }
  return WiFi.softAPIP().toString();
}

String activeMacAddress(const AppConfig &config) {
  if (config.wifiMode == WifiMode::Station && WiFi.status() == WL_CONNECTED) {
    return WiFi.macAddress();
  }
  return WiFi.softAPmacAddress();
}

}  // namespace

NavvyWebServer::NavvyWebServer(ControlLogic &controlLogic)
    : httpServer_(80), wsServer_(81), controlLogic_(controlLogic) {
  instance_ = this;
}

bool NavvyWebServer::begin(const AppConfig &config) {
  config_ = config;

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed, using embedded fallback UI.");
  }

  const bool hasStationCredentials = config_.staSsid[0] != '\0';
  if (config_.wifiMode == WifiMode::Station && hasStationCredentials) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config_.staSsid, config_.staPassword);

    const uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startMs < 10000) {
      delay(100);
    }

    if (WiFi.status() != WL_CONNECTED) {
      WiFi.disconnect(true);
      WiFi.mode(WIFI_AP);
      WiFi.softAP(config_.apSsid, config_.apPassword);
    }
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(config_.apSsid, config_.apPassword);
  }

  httpServer_.on("/", [this]() {
    if (LittleFS.exists("/index.html")) {
      File file = LittleFS.open("/index.html", "r");
      httpServer_.streamFile(file, "text/html");
      file.close();
    } else {
      httpServer_.send_P(200, "text/html", kHtmlPage);
    }
  });

  httpServer_.on("/settings.html", [this]() {
    if (LittleFS.exists("/settings.html")) {
      File file = LittleFS.open("/settings.html", "r");
      httpServer_.streamFile(file, "text/html");
      file.close();
    } else {
      httpServer_.send(404, "text/plain", "settings.html missing");
    }
  });

  httpServer_.on("/preview.html", [this]() {
    if (LittleFS.exists("/preview.html")) {
      File file = LittleFS.open("/preview.html", "r");
      httpServer_.streamFile(file, "text/html");
      file.close();
    } else {
      httpServer_.send(404, "text/plain", "preview.html missing");
    }
  });

  httpServer_.on("/style.css", [this]() {
    if (LittleFS.exists("/style.css")) {
      File file = LittleFS.open("/style.css", "r");
      httpServer_.streamFile(file, "text/css");
      file.close();
    } else {
      httpServer_.send(404, "text/plain", "style.css missing");
    }
  });

  httpServer_.on("/app.js", [this]() {
    if (LittleFS.exists("/app.js")) {
      File file = LittleFS.open("/app.js", "r");
      httpServer_.streamFile(file, "application/javascript");
      file.close();
    } else {
      httpServer_.send(404, "text/plain", "app.js missing");
    }
  });

  httpServer_.on("/api/info", [this]() {
    StaticJsonDocument<384> doc;
    doc["mac"] = activeMacAddress(config_);
    doc["apMac"] = WiFi.softAPmacAddress();
    doc["ip"] = activeIpAddress(config_);
    doc["wifiNetwork"] = activeNetworkName(config_);
    doc["ssid"] = activeNetworkName(config_);
    doc["connectivity"] = (config_.wifiMode == WifiMode::Station && WiFi.status() == WL_CONNECTED) ? "Station" : "Access Point";
    doc["websocket"] = clientConnected() ? "Connected" : "Disconnected";
    doc["firmware"] = "0.1.0";
    doc["leftHanded"] = loadLeftHanded();

    String payload;
    serializeJson(doc, payload);
    httpServer_.send(200, "application/json", payload);
  });

  httpServer_.on("/api/ui", [this]() {
    StaticJsonDocument<96> doc;
    doc["leftHanded"] = loadLeftHanded();

    String payload;
    serializeJson(doc, payload);
    httpServer_.send(200, "application/json", payload);
  });

  httpServer_.on("/api/ui", [this]() {
    StaticJsonDocument<128> request;
    const String body = httpServer_.arg("plain");
    if (body.length() == 0) {
      StaticJsonDocument<96> response;
      response["leftHanded"] = loadLeftHanded();
      String payload;
      serializeJson(response, payload);
      httpServer_.send(200, "application/json", payload);
      return;
    }

    if (deserializeJson(request, body)) {
      httpServer_.send(400, "application/json", "{\"ok\":false}");
      return;
    }

    bool leftHanded = loadLeftHanded();
    if (request.containsKey("leftHanded")) {
      leftHanded = request["leftHanded"] | leftHanded;
    } else if (request.containsKey("handed")) {
      const char *handed = request["handed"] | "right";
      leftHanded = strcmp(handed, "left") == 0;
    }

    const bool ok = saveLeftHanded(leftHanded);
    StaticJsonDocument<96> response;
    response["ok"] = ok;
    response["leftHanded"] = leftHanded;

    String payload;
    serializeJson(response, payload);
    httpServer_.send(ok ? 200 : 500, "application/json", payload);
  });

  httpServer_.on("/api/wifi", [this]() {
    StaticJsonDocument<192> request;
    const String body = httpServer_.arg("plain");
    if (body.length() == 0) {
      StaticJsonDocument<96> response;
      response["ok"] = true;
      response["ssid"] = String(config_.staSsid);
      String payload;
      serializeJson(response, payload);
      httpServer_.send(200, "application/json", payload);
      return;
    }

    if (deserializeJson(request, body)) {
      httpServer_.send(400, "application/json", "{\"ok\":false}");
      return;
    }

    const String ssid = request["ssid"] | "";
    const String password = request["pass"] | "";
    const bool ok = saveWifiSettings(ssid, password);

    StaticJsonDocument<96> response;
    response["ok"] = ok;
    response["ssid"] = ssid;

    String payload;
    serializeJson(response, payload);
    httpServer_.send(ok ? 200 : 500, "application/json", payload);
  });

  httpServer_.on("/health", [this]() {
    httpServer_.send(200, "application/json", "{\"ok\":true}");
  });

  httpServer_.onNotFound([this]() {
    httpServer_.send(404, "text/plain", "Not found");
  });

  httpServer_.begin();

  wsServer_.onEvent(&NavvyWebServer::onWebSocketEvent);
  wsServer_.begin();
  return true;
}

void NavvyWebServer::loop() {
  httpServer_.handleClient();
  wsServer_.loop();
}

void NavvyWebServer::broadcastTelemetry(const String &jsonPayload) {
  wsServer_.broadcastTXT(jsonPayload.c_str());
}

bool NavvyWebServer::takeCommand(WebCommand &commandOut) {
  if (!hasPendingCommand_) {
    return false;
  }

  commandOut = pendingCommand_;
  hasPendingCommand_ = false;
  return true;
}

bool NavvyWebServer::clientConnected() {
  return wsServer_.connectedClients() > 0;
}

void NavvyWebServer::onWebSocketEvent(uint8_t clientId, WStype_t type, uint8_t *payload, size_t length) {
  if (instance_ != nullptr) {
    instance_->handleWebSocketEvent(clientId, type, payload, length);
  }
}

void NavvyWebServer::handleWebSocketEvent(uint8_t clientId, WStype_t type, uint8_t *payload, size_t length) {
  (void)clientId;

  switch (type) {
    case WStype_CONNECTED: {
      WebCommand command;
      command.connected = true;
      queueCommand(command);
      break;
    }
    case WStype_DISCONNECTED: {
      WebCommand command;
      command.connected = false;
      command.disarmRequest = true;
      queueCommand(command);
      break;
    }
    case WStype_TEXT: {
      StaticJsonDocument<256> doc;
      const DeserializationError error = deserializeJson(doc, payload, length);
      if (error) {
        break;
      }

      WebCommand command;
      const char *typeName = doc["type"] | "";
      if (strcmp(typeName, "source") == 0) {
        command.hasSource = true;
        const char *sourceName = doc["source"] | "rc";
        command.source = (strcmp(sourceName, "web") == 0) ? ControlSource::Web : ControlSource::Rc;
      } else if (strcmp(typeName, "arm") == 0) {
        command.armRequest = true;
      } else if (strcmp(typeName, "disarm") == 0) {
        command.disarmRequest = true;
      } else if (strcmp(typeName, "control") == 0) {
        command.hasControl = true;
        command.steeringUs = doc["steeringUs"] | 1500;
        command.throttleUs = doc["throttleUs"] | 1500;
        const char *modeName = doc["mode"] | "joystick";
        command.mode = (strcmp(modeName, "tank") == 0) ? ControlMode::Tank : ControlMode::Joystick;
      }

      queueCommand(command);
      break;
    }
    default:
      break;
  }
}

void NavvyWebServer::queueCommand(const WebCommand &command) {
  pendingCommand_ = command;
  hasPendingCommand_ = true;
}

}  // namespace navvy
