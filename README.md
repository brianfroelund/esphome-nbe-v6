# ESPHome NBE V6 Pellet Stove Controller & Monitor

[![ESPHome Version](https://img.shields.io/badge/ESPHome-2024.6.0%2B-blue.svg)](https://esphome.io)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-green.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![GitHub release](https://img.shields.io/github/v/release/brianfroelund/esphome-nbe-v6?include_prereleases&color=orange)](https://github.com/brianfroelund/esphome-nbe-v6/releases)

An open-source **ESPHome custom external component and firmware** to bridge legacy **NBE Version 6 (Scotte / Woody / Blackstar)** wood pellet burner controllers directly to **Home Assistant** over Wi-Fi.

This project takes inspiration and protocol reverse-engineering from [motoz/PellMon](https://github.com/motoz/PellMon), porting the binary/ASCII framing and XOR checksum protocol to native C++ on ESPHome.

---

## Features & Controls

### 1. Full Real-Time Telemetry (Read)
- **Temperatures:** Boiler, Chute (drop-tube backfire safety), Smoke/exhaust, Boiler return, Domestic hot water (DHW), and Outdoor temperature.
- **Power & Combustion:** Instantaneous burner modulation power (%), calculated power (kW), residual exhaust oxygen (% O2), target oxygen, and optical flame sensor level (%).
- **Timers & Counters:** Auger cycle run-time, electric ignition run-time, lifetime total auger motor hours, and lifetime total electric ignition element hours.
- **Diagnostics:** Human-readable operational mode (`Waiting`, `Starting 1/2`, `Running`, `Paused`, `Making hotwater`, `Stopped`, etc.) and active alarms (`Ignition failed`, `Low boiler temp`, `Max chute temp exceeded`, `Out of pellets`, etc.).

### 2. Comprehensive Burner & Feeding Controls (Read & Write)
- **Burner Power Switch:** Master toggle to turn burner ON (`V01`) or OFF (`V00`).
- **Target Boiler Temperature:** Slider control from $40^\circ\text{C}$ to $85^\circ\text{C}$ (`B01`).
- **Feeding Mechanism Controls ("Snegl"):**
  - **External Auger Calibration Capacity (`F01`):** Adjust the measured pellet dosage in grams for a 6-minute / 360-second test ($400\text{g} - 8000\text{g}$).
  - **Auto-Calculation Switch (`H04`):** Toggle between automatic fuel calculations based on hopper capacity test or manual pulse timing.
  - **Feeder Time Low (`B04`):** Manual pulse duration at 10% load ($0.5\text{s} - 25.0\text{s}$).
  - **Feeder Time High (`B05`):** Manual pulse duration at 100% load ($1.0\text{s} - 100.0\text{s}$).
  - **Feeder Cycles Per Minute (`B06`):** Select between 1, 2, or 3 dosing cycles per minute.
- **Blower Speed Controls ("Blæser hastighed"):**
  - **Blower Low (`A00`):** Fan speed at 10% power ($4\% - 50\%$).
  - **Blower Mid (`A06`):** Fan speed at 50% power ($5\% - 75\%$).
  - **Blower High (`A01`):** Fan speed at 100% power ($5\% - 100\%$).
- **Power Modulation Clamps:**
  - **Min Power (`H02`):** Lower modulation floor ($10\% - 100\%$).
  - **Max Power (`H03`):** Upper modulation ceiling ($10\% - 100\%$).
- **Action Buttons:** Clear/reset active alarm (`V02`) and reset ignition counter (`D04`).

---

## Hardware Architecture & Wiring

```
+--------------------+        +---------------------+        +--------------------+
|                    |        |       MAX3232       |        |       NBE V6       |
|    ESP32 Board     |        |   Transceiver Board |        |    Pellet Stove    |
|                    |        |                     |        |                    |
|  3.3V -------------+--------> VCC                 |        |                    |
|  GND  -------------+--------> GND                 |        |                    |
|                    |        |                     |        |                    |
|  GPIO16 (RX2) <----+--------+ TXD (TTL side)      |        |                    |
|  GPIO17 (TX2) -----+--------> RXD (TTL side)      |        |                    |
|                    |        |                     |        |                    |
|                    |        |  TX-out (Pin 2) ----+--------> RX Terminal        |
|                    |        |  RX-in  (Pin 3) <---+--------+ TX Terminal        |
|                    |        |  GND    (Pin 5) ----+--------> GND Terminal       |
+--------------------+        +---------------------+        +--------------------+
```

> ⚠️ **IMPORTANT ELECTRICAL WARNING:**
> The NBE V6 controller exposes true **RS-232 voltage levels ($\pm 5\text{V}$ to $\pm 12\text{V}$)**.
> **DO NOT wire the controller directly to the 3.3V ESP32 GPIOs.** You must use a **MAX3232** 3.3V RS232-to-TTL module between them.

For complete wiring photos and pinout details, see the [Hardware Wiring Guide](docs/hardware_wiring.md).

---

## Quick Start (Installation)

### 1. Add External Component to Your ESPHome YAML

You can reference this repository directly in your ESPHome configuration:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/brianfroelund/esphome-nbe-v6
      ref: main
    components: [ nbe_v6 ]

uart:
  id: uart_nbe
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600
  stop_bits: 1
  data_bits: 8
  parity: NONE

nbe_v6:
  id: nbe_hub
  uart_id: uart_nbe
  update_interval: 5s

# Enable desired sensors, switches, and number controls:
sensor:
  - platform: nbe_v6
    nbe_v6_id: nbe_hub
    boiler_temperature:
      name: "Boiler Temperature"
    chute_temperature:
      name: "Chute Temperature"
    power_percent:
      name: "Burner Power"

switch:
  - platform: nbe_v6
    nbe_v6_id: nbe_hub
    burner:
      name: "Burner Power"
    autocalculation:
      name: "Feeder Auto-Calculation"

number:
  - platform: nbe_v6
    nbe_v6_id: nbe_hub
    boiler_temp_set:
      name: "Boiler Target Setpoint"
    feeder_capacity:
      name: "External Auger Calibration"
    blower_low:
      name: "Blower Speed Low (10%)"
    blower_mid:
      name: "Blower Speed Mid (50%)"
    blower_high:
      name: "Blower Speed High (100%)"
    feeder_low:
      name: "Feeder Time Low (10%)"
    feeder_high:
      name: "Feeder Time High (100%)"

button:
  - platform: nbe_v6
    nbe_v6_id: nbe_hub
    reset_alarm:
      name: "Reset Alarm"
```

A complete ready-to-flash example configuration is available in [nbe-v6-esp32.yaml](nbe-v6-esp32.yaml).

### 2. Flash and Add to Home Assistant

1. Flash your ESP32 board using ESPHome dashboard or CLI:
   ```bash
   esphome run nbe-v6-esp32.yaml
   ```
2. Once connected to Wi-Fi, Home Assistant will automatically discover the device via the native ESPHome integration.
3. Click **Configure**, enter your encryption key, and all entities will be instantly available in your Home Assistant dashboards.

---

## Protocol Overview

The NBE V6 controller communicates using an 8-bit ASCII protocol at 9600 baud (8N1).
Every message sent to and received from the controller terminates with an XOR checksum byte:
$$\text{Checksum} = \bigoplus_{i=1}^{n} \text{byte}_i$$

- **Poll Requests:**
  - `Z000000` + CS: Primary telemetry frame (100 response bytes + 1 checksum byte).
  - `Z010000` + CS: Combustion and feeder configuration frame (230 response bytes + 1 checksum byte).
  - `Z020000` + CS: Runtime counters frame (40 response bytes + 1 checksum byte).
  - `Z030000` + CS: Auto-calculation and mid-load parameters (100 response bytes + 1 checksum byte).
  - `Z040000` + CS: Firmware version identification (10 response bytes + 1 checksum byte).
  - `Z060000` + CS: External temperature sensors frame (90 response bytes + 1 checksum byte).
- **Control Commands:**
  - `V010000` + CS: Turn burner ON (replies `OK` + CS).
  - `V000000` + CS: Turn burner OFF (replies `OK` + CS).
  - `V020000` + CS: Reset active alarm (replies `OK` + CS).
  - `B01xxxx` + CS: Set target boiler temperature (replies `OK` + CS).
  - `F01xxxx` + CS: Set feeder capacity in grams (replies `OK` + CS).
  - `A00xxxx` / `A06xxxx` / `A01xxxx` + CS: Set blower speeds (replies `OK` + CS).
  - `B04xxxx` / `B05xxxx` / `B06xxxx` + CS: Set feeder pulse times and frequencies (replies `OK` + CS).
  - `H04xxxx` + CS: Enable/disable auto calculation (replies `OK` + CS).

---

## Acknowledgements

- **[motoz/PellMon](https://github.com/motoz/PellMon)** by Anders Nylund: The definitive open-source reference for the Scotte/NBE serial protocol.
- **NBE (Nordjysk Bioenergi)**: Manufacturers of Scotte, Woody, and Blackstar pellet systems.
- **ESPHome Team**: For the flexible IoT framework.

---

## License

This project is licensed under the **GNU General Public License v3.0** (GPL-3.0) — see the [LICENSE](LICENSE) file for details.
