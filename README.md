# ☀️ Sun Tracker — ESP32

An autonomous dual-axis sun tracking system built on the ESP32, using LDR sensors to detect light intensity and a continuous-rotation servo to orient a solar panel toward maximum sunlight.

## 📋 Features
- **Dual LDR light sensing** — compares left vs right light intensity
- **360° continuous rotation servo** — smooth sun tracking
- **OLED display** — real-time LDR values, servo state, system status
- **Buzzer alerts** — audio feedback for limits, errors, calibration
- **Button controls** — mode switching, manual override, calibration
- **Solar-powered** — TP4056 charger + 18650 battery + boost converter
- **Rockers switch** — main power on/off

## 🧩 Components
| Component | Qty | Notes |
|-----------|-----|-------|
| ESP-32S 30Pin Dev Board | 1 | Main MCU |
| MG996R Servo 360° | 1 | Continuous rotation |
| LDR Photoresistor 12mm | 4 | 2 active + 2 spare |
| OLED 0.96" I2C SSD1306 | 1 | 128×64 display |
| Piezo Buzzer 3-24V | 1 | Active buzzer |
| TP4056 Charger + Boost | 1 | Battery management |
| Mini Solar Panel 1.5W 6V | 1 | Power source |
| 18650 Li-ion Battery | 1 | Energy storage |
| Push Buttons (PBS-110) | 6 | User input |
| Rocker Switch (SPST) | 1 | Main power |
| Resistors 10kΩ | 2 | LDR voltage dividers |
| Resistors 220Ω | 2 | TBD |
| Resistors 1kΩ | 2 | TBD |

## 🔌 Wiring Diagram

### Signal Connections
| ESP32 GPIO | Component | Wire/Connection |
|------------|-----------|-----------------|
| **12** | Servo Signal (orange) | PWM |
| **33** | LDR Left (middle of divider) | ADC |
| **34** | LDR Right (middle of divider) | ADC |
| **14** | Buzzer (+) | Digital Output |
| **21** | OLED SDA | I2C Data |
| **22** | OLED SCL | I2C Clock |
| **4** | Button 1 | Input (pull-up) |
| **13** | Button 2 | Input (pull-up) |
| **15** | Button 3 | Input (pull-up) |
| **16** | Button 4 | Input (pull-up) |
| **17** | Button 5 | Input (pull-up) |
| **18** | Button 6 | Input (pull-up) |

### LDR Voltage Divider (×2)
```
3.3V ──[LDR]──┬── GPIO 33 (Left) or 34 (Right)
              │
           [10kΩ]
              │
             GND
```

### Servo Power
```
Servo Orange (Signal) ── GPIO 12
Servo Red    (VCC)    ── 5V (from TP4056 boost OUT+)
Servo Brown  (GND)    ── Common GND
```

### Power Chain
```
Solar Panel (+) ──→ TP4056 IN+
Solar Panel (-) ──→ TP4056 IN-

TP4056 B+ / B- ──→ 18650 Battery

TP4056 OUT+ ──→ Rocker Switch ──→ ESP32 VIN (5V)
                                    Servo VCC (5V)
TP4056 OUT- ──→ Common GND
```

### OLED I2C
```
OLED VCC ── 3.3V or 5V
OLED GND ── GND
OLED SCL ── GPIO 22
OLED SDA ── GPIO 21
```

### Buzzer
```
Buzzer (+) ── GPIO 14
Buzzer (-) ── GND
```

### Buttons (all 6 — same wiring)
```
ESP32 GPIO ──→ Button leg 1
Button leg 2 ──→ GND
(Internal pull-up enabled in code)
```

## 🛠️ Setup Guide

### 1. Arduino IDE Setup
1. Install [Arduino IDE](https://www.arduino.cc/en/software) (2.x recommended)
2. Add ESP32 board manager:
   - **File → Preferences → Additional Boards Manager URLs:**
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - **Tools → Board → Boards Manager →** Search "ESP32" → Install **esp32 by Espressif Systems**

3. Select board:
   - **Tools → Board → ESP32 Arduino → ESP32 Dev Module**
   - **Tools → Upload Speed → 921600** (or 115200)
   - **Tools → Port →** Select your ESP32 COM port

### 2. Install Libraries
**Sketch → Include Library → Manage Libraries →** Search & install:

| Library | Author |
|---------|--------|
| `ESP32Servo` | John K. Bennett et al. |
| `Adafruit SSD1306` | Adafruit |
| `Adafruit GFX Library` | Adafruit |

Or via CLI:
```bash
arduino-cli lib install ESP32Servo Adafruit_SSD1306 Adafruit_GFX_Library
```

### 3. Build the Circuit
1. Place ESP32 on breadboard
2. Wire LDR voltage dividers on both sides
3. Connect servo, OLED, buzzer
4. Wire buttons to GND (pull-up in code)
5. Connect TP4056 power chain (solar → battery → boost → ESP32 VIN)
6. Add rocker switch between boost OUT+ and ESP32 VIN

### 4. Upload Code
1. Open `Sun_Tracker.ino` in Arduino IDE
2. Connect ESP32 via USB
3. Hold **BOOT** button, press **EN** (reset), release BOOT (if upload fails)
4. Click **Upload**

### 5. Calibrate LDRs
See [`docs/CALIBRATION.md`](docs/CALIBRATION.md) for LDR threshold calibration.

## 🎮 Operation

### Modes (TBD — button functions pending)
| Mode | Behavior |
|------|----------|
| **Auto** | Servo tracks sun automatically based on LDR comparison |
| **Manual** | Buttons control servo direction |
| **Off** | Servo stopped, system idle |

### OLED Display
Shows real-time:
- Left & Right LDR values (0–4095)
- Servo direction indicator (← ■ →)
- Current mode
- Alerts

### Buzzer
- **Short beep** — startup / mode change
- **Continuous** — limit reached / error

## 📁 Project Structure
```
Sun_Tracker/
├── Sun_Tracker.ino      # Main firmware
├── components/           # Component datasheets & specs
│   ├── ESP32.md
│   ├── Servo360.md
│   ├── LDR.md
│   ├── Buzzer.md
│   ├── OLED.md
│   ├── Buttons.md
│   ├── TP4056.md
│   ├── SolarPanel.md
│   ├── RockerSwitch.md
│   └── Resistors.md
├── docs/
│   ├── WIRING.md         # Detailed wiring guide
│   ├── SETUP.md          # Step-by-step setup
│   └── CALIBRATION.md    # LDR calibration process
├── circut.png            # Circuit diagram
└── README.md             # This file
```

## ⚠️ Notes
- **GPIO 34-39 are input-only** on ESP32 — no internal pull-up. LDRs are fine (voltage divider).
- **Servo needs external 5V** — ESP32 5V pin may not source enough current. Use TP4056 boost output.
- **TP4056 has no battery protection** — monitor battery voltage, don't drain below 3.0V.
- **Solar panel output varies** — 1.5W / 250mA is slow charging. Expect longer charge times on cloudy days.

## 📝 TODO
- [ ] Assign button functions (mode toggle, calibrate, manual control, etc.)
- [ ] Implement sun tracking algorithm
- [ ] Implement button handling with debounce
- [ ] OLED display layout
- [ ] LDR calibration routine
- [ ] Buzzer alert patterns
- [ ] Power consumption optimization

---
Built for Embedded Systems Course — Year 1, Semester 2
