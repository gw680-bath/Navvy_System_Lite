Navvy Control Lite — Web UI

Files added to `data/` (SPIFFS):
- index.html — Main control UI (joystick, tank, arm, telemetry)
- settings.html — Device info, WiFi form, connectivity info
- style.css — Lightweight light/dark theme and layout
- app.js — Client-side WebSocket + UI logic

Notes for integration
- Serve these files from SPIFFS or embedded filesystem. PlatformIO "data/" folder will be uploaded to SPIFFS/LittleFS.
- WebSocket endpoint expected at `/ws` that accepts JSON messages and sends telemetry JSON like:
  {"telemetry":{"armed":true,"source":"web","rcOk":false,"steering":0.12,"throttle":-0.3,"battery":12.7}}
- Info endpoint expected at `/api/info` returning JSON `{mac,ip,ssid,fw}`.
- WiFi save endpoint expected at `POST /api/wifi` with JSON `{ssid,pass}` returning `{ok:true}` on success.

Next steps
- Hook the server-side WebSocket and REST endpoints in `WebServer.cpp`.
- Optionally refine UI visuals or add joystick deadzone and smoothing.
