# navvy_system_lite

PlatformIO / Arduino framework scaffold for an ESP32-WROOM bridge between an RC receiver and a Roboclaw 2x60A motor controller.

## What is included

- PWM input capture for RC CH1/CH2
- LEDC-based servo PWM output
- Control logic with arm/disarm safety and source selection
- Embedded web UI with WebSocket telemetry/control
- NVS-backed configuration helper
- Modular source layout for future expansion

## Project layout

- `src/PwmInput.h/.cpp` - RC pulse capture
- `src/PwmOutput.h/.cpp` - Servo PWM output
- `src/ControlLogic.h/.cpp` - Arm/failsafe/source arbitration
- `src/WebServer.h/.cpp` - Embedded HTTP + WebSocket UI
- `src/Telemetry.h/.cpp` - JSON telemetry formatting and pacing
- `src/Config.h` - Pins, defaults, and NVS config storage
- `src/main.cpp` - Runtime wiring

## Hardware defaults

- RC input CH1: GPIO 34
- RC input CH2: GPIO 35
- PWM out steer: GPIO 25
- PWM out throttle: GPIO 26
- Battery sense: GPIO 36

The ESP32 GPIOs are not 5 V tolerant. Level shifting or an appropriate receiver output stage is required.

## Build

1. Open `navvy_system_lite` in VS Code.
2. Install PlatformIO.
3. Build with the PlatformIO toolbar or `pio run`.

## Notes

- The web UI is embedded in firmware, so no SPIFFS upload is required.
- The project starts as an access point by default and can be expanded to station mode later.
- Roboclaw serial control can replace the PWM output module later without changing the control logic interface.
