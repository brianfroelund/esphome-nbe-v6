# Hardware Wiring & Interface Guide for NBE V6

This guide provides the complete hardware wiring and setup instructions for connecting an ESP32 (or ESP8266) to an **NBE Version 6 controller** (Scotte / Woody / Blackstar pellet burners).

---

## ⚠️ Critical Safety & Electrical Warning

> **DO NOT connect the NBE V6 serial port directly to the ESP32 GPIO pins!**
> 
> The NBE V6 controller communicates via standard **RS-232 voltage levels ($\pm 5\text{V}$ to $\pm 12\text{V}$)**. Direct connection to the $3.3\text{V}$ logic of an ESP32 or ESP8266 will permanently destroy the microcontroller.
> 
> A **MAX3232 RS232-to-TTL transceiver module** powered from the ESP's $3.3\text{V}$ line is **strictly required**.

---

## Bill of Materials (BOM)

### Option A: External Mounting (Standard / Easiest)
| Component | Description | Approximate Cost |
| :--- | :--- | :--- |
| **ESP32 DevKit v1** (or ESP32-C3) | Microcontroller board with Wi-Fi and multiple hardware UARTs | ~$3 - $5 (25 - 40 DKK) |
| **MAX3232 Module** | 3.3V RS-232 to TTL converter board (with DB9 connector or screw terminals) | ~$1 - $2 (10 - 20 DKK) |
| **5V USB Power Supply** | 5V 1A/2A DC USB power adapter and cable | ~$3 - $5 |
| **Connecting Wires** | DuPont jumper wires / 3-conductor shielded cable | ~$1 |

### Option B: All-in-One Internal Mounting (Clean, Inside NBE Box)
| Component | Description | Approximate Cost |
| :--- | :--- | :--- |
| **ESP32 Board** | ESP32-C3 SuperMini or ESP32 DevKit | ~$3 - $5 |
| **MAX3232 Module** | 3.3V RS-232 to TTL converter board | ~$1 - $2 |
| **Hi-Link HLK-PM01** | Mini isolated 230VAC to 5V DC step-down module (3W / 600mA) | ~$2 - $3 (15 - 25 DKK) |
| **Optional External Antenna** | ESP32 with IPEX/U.FL connector + pigtail antenna (if metal box shields Wi-Fi) | ~$2 |

---

## Internal Mounting & "Stealing Power": What You Must Know

Many users wish to fit the ESP32 completely inside the NBE controller box for a tidy installation without external cables.

### 1. Physical Space
- **Yes**, there is ample space inside the NBE V6 controller enclosure to mount an ESP32, MAX3232, and a small power supply module.
- **Wi-Fi Shielding (Faraday Cage)**: The NBE enclosure is metal and mounted next to a large metal boiler. If your Wi-Fi signal in the boiler room is borderline, the metal box can attenuate the 2.4 GHz signal.
  - *Solution*: Use an ESP32 variant with an external antenna connector (IPEX/U.FL) and route the antenna through an existing rubber cable gland or knock-out hole, or test with the lid on before sealing.

### 2. Can You Steal Power from the NBE Low-Voltage Rails (5V / 12V)?
- **❌ DO NOT tap the NBE board's internal 5V logic rail!**
  - The onboard 5V regulator (linear 7805 fed by a small ~2VA transformer) was designed in the 2000s strictly to power the low-power MCU (10–30 mA), sensor ADC, and 7-segment LED display.
  - The ESP32 draws **200–350 mA (with RF transmission bursts up to 450–500 mA)**.
  - Drawing this from the NBE 5V rail will overheat the onboard regulator or cause brown-out voltage dips that crash or reset the burner controller mid-burn!
- **✅ The Safe Solution: Tap the 230V Mains Terminals Inside the Box!**
  - Inside the NBE case, 230V AC mains enters via screw terminals (L, N, PE).
  - Connect a tiny, fully isolated **Hi-Link HLK-PM01 (230VAC to 5V DC)** module to the 230V incoming terminals (downstream of the main switch/fuse).
  - Connect the 5V DC output of the HLK-PM01 directly to the `5V` / `VIN` and `GND` pins of the ESP32.
  - This keeps the stove's sensitive microchip 100% isolated, completely eliminates brown-outs, and gives you a clean internal installation with zero external power bricks.

---

## Wiring Pinout Diagram

```
+---------------------------------------------------------------------------------+
| NBE V6 Controller Box (Internal)                                                |
|                                                                                 |
| 230VAC Mains IN ---+---> [Hi-Link HLK-PM01]                                     |
|                    |     5V DC OUT ---+-------------> ESP32 (5V / VIN)          |
|                    |     GND       ---+-------------> ESP32 (GND)               |
|                                                            |                    |
|                                                3.3V OUT ---+---> MAX3232 (VCC)  |
|                                                GND      ---+---> MAX3232 (GND)  |
|                                                                     |           |
|                                            GPIO16 (RX2) <-------+-- TXD (TTL)   |
|                                            GPIO17 (TX2) --------+-> RXD (TTL)   |
|                                                                     |           |
| NBE Serial Screw Terminals:                                         |           |
|   RX Terminal <------------------------------------------------- TX (RS-232)   |
|   TX Terminal -------------------------------------------------> RX (RS-232)   |
|   GND Terminal ------------------------------------------------- GND           |
+---------------------------------------------------------------------------------+
```

### ESP32 to MAX3232 (TTL Side, 3.3V)
| ESP32 Pin | MAX3232 Pin | Function |
| :--- | :--- | :--- |
| **3V3** | **VCC** | Power transceiver module at 3.3V logic levels |
| **GND** | **GND** | Common Ground |
| **GPIO16** (UART2 RX) | **TXD** (TTL TX out) | Receives data from MAX3232 into ESP |
| **GPIO17** (UART2 TX) | **RXD** (TTL RX in) | Transmits data from ESP into MAX3232 |

### MAX3232 to NBE V6 (RS-232 Side)
| MAX3232 (or DB9 Female) | NBE V6 Controller Terminal | Note |
| :--- | :--- | :--- |
| **TX** (DB9 Pin 2 / RXD in standard null modem) | **RX** on NBE board | Transmits commands to stove |
| **RX** (DB9 Pin 3 / TXD in standard null modem) | **TX** on NBE board | Receives telemetry from stove |
| **GND** (DB9 Pin 5) | **GND** on NBE board | Signal ground reference |

*(Note: If no communication is received on initial power-up, swap TX and RX between the MAX3232 RS-232 side and the NBE controller terminals. Crossing TX/RX is the most common initial setup oversight.)*

---

## Testing & Troubleshooting

1. **Power Sequence:**
   - Power the ESP32.
   - Ensure the NBE V6 stove controller is turned on.
2. **Check Logs:**
   - Run `esphome logs nbe-v6-esp32.yaml`.
   - On startup, the component transmits `"Z040000"` to detect the chip version, followed by periodic `"Z000000"` telemetry polls.
   - Successful communication will log:
     ```
     [I][nbe_v6:...] Detected NBE V6 chip version: 6.78
     [V][nbe_v6:...] Valid frame received for [Telemetry (Z00)]
     ```
3. **If You Experience Timeouts:**
   - Swap the TX and RX wires between MAX3232 and NBE V6.
   - Verify that the MAX3232 is powered from **3.3V**, not 5V.
   - Ensure the RS-232 chip on the NBE mainboard is firmly seated in its socket.
