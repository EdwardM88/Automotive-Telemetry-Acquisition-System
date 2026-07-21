# Automotive-Telemetry-Acquisition-System

An ESP32-based anti-theft and telemetry prototype for a vehicle, combining motion
detection, GPS tracking, CAN bus communication, and a live status display —
paired with an Arduino Uno acting as an engine/RPM simulator for bench testing.

## Overview

The system is split into two boards:

| Board | Role |
|---|---|
| **ESP32** | Main controller: reads accelerometer + GPS, talks to the CAN bus, drives the OLED display, handles deep sleep |
| **Arduino Uno** | Bench simulator: generates a fake engine pulse (RPM) signal so the ESP32 firmware can be developed/tested without a real vehicle |

## Hardware

- ESP32 dev board
- ADXL345 accelerometer (I2C) — shock / motion detection
- NEO-6M GPS module (UART2) — location + speed
- SSD1306 OLED display (I2C) — live status
- MCP2515 CAN Bus module (SPI) — vehicle bus interface
- Arduino Mega 2560 (separate board) — engine/RPM simulator: push button, potentiometer as a "throttle", with debounce and
  simple inertia/jitter simulation

### Wiring (ESP32)

| Peripheral | Pins |
|---|---|
| OLED / ADXL345 (I2C) | SDA = GPIO 21, SCL = GPIO 22 |
| GPS (UART2) | RX = GPIO 17, TX = GPIO 16 |
| MCP2515 (SPI) | CS = GPIO 15 |
| ADXL345 INT1 | GPIO 14 (reserved for a future shock-alarm feature) |
| Pulse input from Arduino simulator | GPIO 33 (RTC-capable, used for `ext0` deep-sleep wakeup) |

> ⚠️ Make sure the ESP32 and the Arduino simulator share a **common GND** —
> without it, the pulse signal read on GPIO 33 can be unreliable.

## Circuit diagram
<img width="1169" height="827" alt="Schematic_Schema_CarProjectV1_2026-07-20" src="https://github.com/user-attachments/assets/31301fc3-5149-4e53-9a9e-3fa4c031dfa1" />


## CAN bus mode — important

The MCP2515 module is currently initialized in **`MCP_LOOPBACK`** mode. This is
intentional for bench-testing: the ESP32 sends its own RPM/speed frame and
immediately reads it back, so what you see on the OLED is a loopback echo —
**not** real data from a vehicle bus.

Before installing on a real car, switch the mode in `setupCAN()`:

```cpp
CAN0.setMode(MCP_NORMAL);      // read/write on the real bus
// or
CAN0.setMode(MCP_LISTENONLY);  // read-only, never transmits
```

## Features

- Real-time RPM calculation from pulse period, with basic noise filtering
- GPS speed, location, and satellite count via TinyGPS++
- Shock/impact detection via ADXL345 accelerometer
- RPM + speed broadcast over CAN bus (currently loopback for testing)
- Live status on a 128x64 OLED display
- JSON telemetry streamed over Serial once per second
- Deep sleep after a configurable timeout with no engine pulses, woken up by
  the next pulse (`ext0` wakeup on GPIO 33)

## Building / Flashing

1. Open `` in the Arduino IDE
   (or PlatformIO).
2. Install the required libraries: `mcp_can`, `Adafruit_GFX`,
   `Adafruit_SSD1306`, `ArduinoJson`, `Adafruit_ADXL345_U`, `TinyGPS++`.
3. Select your ESP32 board, set the correct COM port, and upload.
4. (Optional) Flash `` to a separate Arduino Uno
   to simulate engine pulses without a real vehicle.

## Roadmap / ideas

- Wire up the ADXL345 INT1 pin to trigger a shock/theft alarm
- Switch CAN mode to `MCP_NORMAL` and validate against a real vehicle bus
- Add persistent logging (SD card or cloud upload) for the JSON telemetry
- Add a buzzer/GSM module for real theft alerts

## License

This project is licensed under the [MIT License](LICENSE).
