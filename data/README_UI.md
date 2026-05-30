Navvy Control Lite — Web UI

Files added to `data/` (SPIFFS):
- index.html — Main control UI (joystick, tank, arm, telemetry)
- settings.html — Device info, WiFi form, connectivity info
- style.css — Lightweight light/dark theme and layout
- app.js — Client-side WebSocket + UI logic
- preview.html — Local preview of the main screen
- settings-preview.html — Local preview of the settings screen

Notes for integration
- Serve these files from LittleFS. The firmware now routes `/`, `/settings.html`, `/style.css`, and `/app.js` from the filesystem.
- WebSocket endpoint is `ws://<host>:81/` and sends flat telemetry JSON like:
  `{ "armed": true, "source": 2, "webOk": true, "steeringUs": 1500, "throttleUs": 1500, "batteryV": 12.7 }`
- Info endpoint is `/api/info` and returns device/network details plus `leftHanded`.
- Handedness endpoint is `/api/ui` with GET/POST JSON `{leftHanded:true|false}`.
- WiFi save endpoint is `/api/wifi` with JSON `{ssid,pass}` returning `{ok:true}` on success.
- The preview files are for browser-only inspection and use relative paths so they can be opened directly with `file://`.

Next steps
- Upload the filesystem image after flashing the firmware.
- If you want, I can also add a tiny boot-time splash or a save confirmation toast.
