
# 📘 ESP32 LILYGO T-Eth Lite: NFC to OSC Bridge Firmware

**Version:** 1.0  
**Target Hardware:** LILYGO T-Eth Lite (ESP32) with Adafruit PN532 NFC Module  
**Communication:** Ethernet (OSC over UDP) + Bluetooth Serial  

---

## 🔧 Overview

This firmware enables an ESP32-based LILYGO T-Eth Lite board to act as a smart bridge between NFC tags and external devices using Open Sound Control (OSC) messages over Ethernet. It also features Bluetooth serial configuration for wireless setup.

Use cases include interactive installations, holobox control, multimedia triggers, and other real-time programmable experiences using NFC tags.

---

## 📦 Hardware Requirements

- LILYGO T-Eth Lite ESP32 board  
- Adafruit PN532 NFC Reader (connected via I2C or IRQ+RESET)  
- Ethernet cable + connection  
- Bluetooth Serial terminal app (e.g., Serial Bluetooth Terminal)
- Optional: POE Switch or injector for power over Ethernet
- Optional: NFC tags (MIFARE ISO14443A)

---

## 📂 File Overview

| File | Description |
|------|-------------|
| `main.cpp` | Main firmware logic: NFC reading, OSC messaging, Bluetooth setup |
| `eth_properties.h` | Pin mapping and Ethernet PHY configuration |
| `platformio.ini` | PlatformIO configuration for building the project |

---

## 🛠️ Features

- Ethernet-based OSC output with configurable IP and ports
- Bluetooth Serial configuration interface (no need to reflash)
- Tag removal detection and custom command trigger
- Watchdog-based auto-recovery for hangs or disconnection
- Persistent configuration via NVS flash
- Dynamic NFC tag assignment using `std::vector`

---

## 🔌 Wiring Guide

| Signal | Pin | Description |
|--------|-----|-------------|
| SDA    | GPIO14 | I2C Data for PN532 |
| SCL    | GPIO32 | I2C Clock for PN532 |
| IRQ    | GPIO2  | IRQ pin for Adafruit PN532 |
| RESET  | GPIO3  | Reset pin for Adafruit PN532 |
| MDC    | GPIO23 | Ethernet MDC |
| MDIO   | GPIO18 | Ethernet MDIO |
| ETH Power | GPIO12 | Enable pin for Ethernet PHY |

> 💡 Ensure proper 3.3V power levels and common ground between devices.

---

## ⚙️ Initialization Flow

```cpp
setup():
 ├── Serial & Bluetooth initialized
 ├── Watchdog initialized
 ├── Preferences loaded
 ├── NFC initialized
 ├── Ethernet initialized
```

---

## 🔁 Main Loop Logic

```cpp
loop():
 ├── Feed watchdog
 ├── readNFC() – Scan for new tags or removals
 ├── readBTSerial() – Accept commands via Bluetooth
```

---

## 💬 OSC Messaging Logic

### When a tag is placed:

```cpp
timeCodeOSCSend(mode);
```
Sends:
```
/modeX [int]
```

### When a tag is removed:

```
Serial.println(removeCommand); // For logging or triggering externally
```

---

## 🎮 Bluetooth Configuration Commands

Send via Bluetooth (e.g., Serial Bluetooth Terminal):

| Command | Description |
|---------|-------------|
| `HELP` | Show help commands |
| `N<num>` | Set number of NFC tags (max 20) |
| `T<index>` | Assign last scanned tag to index |
| `C<index><command>` | Set command for tag index |
| `R<command>` | Set message for tag removal |
| `SET_IP <ip>` | Set local IP address |
| `SET_SUBNET <subnet>` | Set subnet mask |
| `SET_GATEWAY <gateway>` | Set network gateway |
| `SET_OUTIP <ip>` | Set destination OSC IP |
| `SET_INPORT <port>` | Set local UDP port |
| `SET_OUTPORT <port>` | Set destination UDP port |
| `SET_MODE <index><value>` | Set OSC mode value for tag |
| `IP` | Print current IP |
| `MAC` | Print MAC address |
| `GET` | Print entire config summary |

---

## 💾 Persistent Storage (NVS)

| Key | Description |
|-----|-------------|
| `numTags` | Number of stored tags |
| `tagX`, `commandX`, `modeX` | X-th tag ID, command, OSC mode |
| `ip0-3`, `sub0-3`, `gw0-3`, `out0-3` | IP settings |
| `inPort`, `outPort` | UDP ports |

---

## 🧪 Debugging & Monitoring

- Monitor via Serial @ `115200`
- Bluetooth Name: **Mini Holotube**
- `#define DEBUG 1` prints extra info (tag ID, config, etc.)

---

## 💡 Best Practices

- Use `GET` to verify current settings
- Send `N<num>` before assigning tags/commands
- Use watchdog + restart logic to recover from issues
- Ethernet disconnection auto-triggers `ESP.restart()`
- NFC initialization failure also triggers auto-restart

---

## 🧠 Future Roadmap

- Add Web-based config interface
- Send OSC on tag **removal**
- MQTT/OSC hybrid support
- Remote OTA via Web or Bluetooth
