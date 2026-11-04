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

| Component | Description | Approximate Cost |
| :--- | :--- | :--- |
| **ESP32 DevKit v1** (or ESP32-C3) | Microcontroller board with Wi-Fi and multiple hardware UARTs | ~$3 - $5 (25 - 40 DKK) |
| **MAX3232 Module** | 3.3V RS-232 to TTL converter board (with DB9 connector or screw terminals) | ~$1 - $2 (10 - 20 DKK) |
| **5V USB Power Supply** | 5V 1A/2A DC USB power adapter and cable for the ESP32 | ~$3 - $5 |
| **Connecting Wires** | DuPont jumper wires / 3-conductor shielded cable | ~$1 |

---

## NBE V6 Controller Board RS-232 Socket & Connectors

1. **Verify RS-232 Driver Chip:**
   - Look at your NBE V6 circuit board. There is a dedicated DIP-16 socket for the RS-232 transceiver chip (MAX232 or equivalent).
   - Ensure the chip is present in its socket. If your board was supplied with the chip, make sure the notch matches the socket silkscreen marking.

2. **Controller Connection Terminals:**
   - Most NBE V6 boards expose serial communication via screw terminals or a female DB9 port:
     - **TX** (Transmit data out from stove)
     - **RX** (Receive data into stove)
     - **GND** (Signal Ground)

---

## Wiring Pinout Diagram

```
+------------------+         +-------------------+         +-------------------+
|                  |         |      MAX3232      |         |      NBE V6       |
|      ESP32       |         |    Transceiver    |         |    Pellet Stove   |
|                  |         |                   |         |                   |
| 3.3V ------------+---------> VCC               |         |                   |
| GND  ------------+---------> GND               |         |                   |
|                  |         |                   |         |                   |
| GPIO16 (RX2) <---+---------+ TXD               |         |                   |
| GPIO17 (TX2) ----+---------> RXD               |         |                   |
|                  |         |      RS-232 Side  |         |   RS-232 Side     |
|                  |         |  TX-out (Pin 2) --+---------> RX Terminal       |
|                  |         |  RX-in  (Pin 3) <-+---------+ TX Terminal       |
|                  |         |  GND    (Pin 5) --+---------> GND Terminal      |
+------------------+         +-------------------+         +-------------------+
```

### ESP32 to MAX3232 (TTL Side, 3.3V)
| ESP32 Pin | MAX3232 Pin | Function |
| :--- | :--- | :--- |
| **3V3** | **VCC** | Power module at 3.3V logic levels |
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
   - Power the ESP32 via USB.
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
