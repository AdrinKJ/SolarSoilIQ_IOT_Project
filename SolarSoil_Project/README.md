# SolarSoil — IoT Solar-Powered Soil Monitoring System

A two-node wireless sensor network built on Heltec WiFi LoRa 32 (V3) boards. A solar-powered sensor node (Node A) reads soil moisture, temperature, humidity, and power metrics, then transmits the data via LoRa to a gateway node (Node B) that displays it live on an OLED screen.

## Project Overview

- **Node A (Sensor Node):** Reads environmental data and transmits it wirelessly every 10 seconds.
- **Node B (Gateway Node):** Receives LoRa packets and displays live readings on its OLED.

## Hardware

### Node A — Sensor Node
- Heltec WiFi LoRa 32 (V3)
- LoRa 868MHz antenna
- DHT22 temperature & humidity sensor
- Capacitive Soil Moisture Sensor v1.2
- INA219 current/power monitor
- Solar panel (6V)
- 4x AA NiMH battery pack
- LM2596 buck converter
- 1N5819 Schottky diode
- 100nF ceramic capacitor (decoupling)

### Node B — Gateway Node
- Heltec WiFi LoRa 32 (V3)
- LoRa 868MHz antenna
- USB powered (no battery/solar needed)

## Pin Connections — Node A

| Component | Pin | Heltec Pin |
|---|---|---|
| DHT22 | VCC | 3V3 (J3, Pin 3) |
| DHT22 | GND | GND (J3, Pin 1) |
| DHT22 | DATA | GPIO 4 (J3, Pin 15) |
| Soil Sensor | VCC | 3V3 |
| Soil Sensor | GND | GND |
| Soil Sensor | AOUT | GPIO 7 (J3, Pin 9) |
| INA219 | VCC | 3V3 |
| INA219 | GND | GND |
| INA219 | SDA | GPIO 33 (separate I2C bus, see note below) |
| INA219 | SCL | GPIO 34 (separate I2C bus, see note below) |

**Important note on I2C:** The Heltec's onboard OLED uses its own dedicated I2C bus (`SDA_OLED`, `SCL_OLED`). The INA219 must be initialized on a **separate I2C bus instance** (`TwoWire(1)`) using GPIO 33/34, otherwise it conflicts with the display and the OLED will not render anything. See `NodeA_Transmitter.ino` for the working implementation.

## Soil Moisture Calibration

Calibrated using `map()` against these raw ADC readings:
- Dry (air): 2514
- Moist (finger touch): 1831
- Wet (water): 1464

Formula used: `map(soilRaw, 2514, 1464, 0, 100)`, constrained to 0–100%.

## Software Setup

1. Install **Arduino IDE**.
2. Add Heltec board support via **File → Preferences → Additional Boards Manager URLs**:
   ```
   https://resource.heltec.cn/download/package_heltec_esp32_index.json
   ```
3. Install via **Boards Manager**: "Heltec ESP32"
4. Install via **Library Manager**:
   - "DHT sensor library" (Adafruit)
   - "INA219" (Adafruit)
   - "Heltec ESP32" (for `LoRaWan_APP.h`)
5. Select board: **Tools → Board → Heltec ESP32 Arduino → Heltec WiFi LoRa 32(V3)**
6. Install the **Silicon Labs CP210x USB driver** if the board doesn't show up under Tools → Port.

## Known Issues / Gotchas

- **OLED stays blank:** Make sure `VextON()` is called before `display.init()` — the Heltec V3 OLED is powered through the Vext rail, which is off by default.
- **OLED blank after adding INA219:** Caused by I2C bus conflict — fixed by giving INA219 its own `TwoWire(1)` instance instead of sharing the default `Wire`.
- **DHT22 occasionally returns NaN:** Normal sensor behavior; the next reading typically recovers automatically.
- **GPIO36 does not work as analog input on this board:** It's mapped to `Vext_Ctrl` on the Heltec V3/V4 pin layout — use a free GPIO like GPIO 7 instead.

## Status

- [x] Node A sensors wired and calibrated (DHT22, Soil Moisture, INA219)
- [x] Node A → Node B LoRa transmission confirmed working
- [x] OLED displays on both nodes showing live data
- [x] Decoupling capacitor added for RF noise protection
- [ ] Power system (solar → diode → battery → buck converter) — pending DC barrel jack adapter
- [ ] Weatherproof enclosure
- [ ] Full field deployment test

## Files

- `NodeA_Transmitter.ino` — Sensor node firmware
- `NodeB_Gateway.ino` — Gateway node firmware
