
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

- Ethernet-based OSC output
- Bluetooth Serial Configuration interface
- Dynamic NFC tag management
- Configurable tag commands and OSC routes
- Persistent configuration storage via NVS Preferences

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
 ├── Preferences (NVS) loaded
 ├── NFC (PN532) initialized
 ├── Ethernet initialized
```

---

## 🔁 Main Loop Logic

```cpp
loop():
 ├── readNFC() – Scan for new tags
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
Serial.println(removeCommand); // Only Serial, not OSC
```

---

## 🎮 Bluetooth Configuration Commands

Send via Bluetooth terminal like Serial Bluetooth Terminal:

| Command | Description |
|---------|-------------|
| `HELP` | Show command list |
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
| `SET_MODE <index><value>` | Set OSC message mode for tag |
| `IP` | Show current IP |
| `MAC` | Show MAC address |
| `GET` | Get current config summary |

> 🟢 Example:
```
SET_IP 192.168.0.10
SET_OUTIP 192.168.0.100
N3
T1
C1HELLO
SET_MODE 01 2
```

---

## 💾 Configuration Persistence

Data is stored in NVS under the `RFID` namespace:

| Data | Key Format |
|------|-------------|
| IP, Subnet, Gateway | `ip0`, `ip1`, ... |
| Tag IDs | `tag0`, `tag1`, ... |
| Commands | `command0`, `command1`, ... |
| OSC Modes | `mode0`, `mode1`, ... |

---

## 📡 OSC Message Format

### Tag Detected:
```
Address: /modeX
Data: [int]
```

### Tag Removed:
```
Printed via Serial only: <removeCommand>
```

---

## 🧪 Debugging & Monitoring

- Serial monitor @ `115200 bps`
- Bluetooth device name: **Mini Holotube**
- Enable/Disable debug prints via `#define DEBUG 1`

---

## 📋 Best Practices

- Save configuration before power-off using `SAVE`-related commands
- Use `GET` regularly to verify setup
- Cap `numTags` to 20 to avoid memory overflows
- Default network config (if not stored):
  - IP: `10.255.250.150`
  - Subnet: `255.255.254.0`
  - Gateway: `10.255.250.1`
  - Out IP: `10.255.250.129`

---

## 🧠 Future Enhancements

- Add OSC message on tag removal
- Add Web Serial or OTA configuration
- Use MDNS for device discovery
- Expand tag capacity with dynamic memory
