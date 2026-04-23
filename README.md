# Solar Tracker — ESP32

An autonomous sun tracking system built on the ESP32, using LDR sensors to detect light intensity and a continuous-rotation servo to orient a solar panel toward maximum sunlight.

## Features

- **Dual LDR light sensing** — compares left vs right light intensity
- **360 degree continuous rotation servo** — smooth sun tracking
- **OLED display** — animated real-time LDR values, servo state, system status
- **Buzzer** — rhythmic tone feedback for all user interactions
- **3-button menu navigation** — LEFT/RIGHT/SELECT with animated OLED menu
- **Solar-powered** — TP4056 charger + 18650 battery + boost converter
- **Deep sleep** — automatic night detection, 1000x power reduction
- **BLE ready** — design documented for future remote monitoring

## Components

| Component                  | Qty | Notes                |
| -------------------------- | --- | -------------------- |
| ESP-32S 30Pin Dev Board    | 1   | Main MCU             |
| MG996R Servo 360 degree    | 1   | Continuous rotation  |
| LDR Photoresistor 12mm     | 4   | 2 active + 2 spare   |
| OLED 0.96 inch I2C SSD1306 | 1   | 128x64 display       |
| Piezo Buzzer 3-24V Active  | 1   | Fixed 3.9kHz tone    |
| TP4056 Charger + Boost     | 1   | Battery management   |
| Mini Solar Panel 1.5W 6V   | 1   | Power source         |
| 18650 Li-ion Battery       | 1   | Energy storage       |
| Push Buttons (PBS-110)     | 3   | LEFT, RIGHT, SELECT  |
| Rocker Switch (SPST)       | 1   | Main power           |
| Resistors 10k ohm          | 2   | LDR voltage dividers |
| Resistors 220 ohm          | 2   | TBD                  |
| Resistors 1k ohm           | 2   | TBD                  |

## Wiring

Full wiring reference is in [`docs/PINOUT.md`](docs/PINOUT.md).

### Signal Connections

| ESP32 GPIO | Component     | Direction        |
| ---------- | ------------- | ---------------- |
| **4**      | Button LEFT   | Input (PULLUP)   |
| **12**     | Servo Signal  | Output (PWM)     |
| **13**     | Button RIGHT  | Input (PULLUP)   |
| **14**     | Buzzer (+)    | Output (Digital) |
| **15**     | Button SELECT | Input (PULLUP)   |
| **21**     | OLED SDA      | I2C Data         |
| **22**     | OLED SCL      | I2C Clock        |
| **33**     | LDR Left      | Input (ADC)      |
| **34**     | LDR Right     | Input (ADC)      |

### LDR Voltage Divider (x2)

```
3.3V --[LDR]--+-- GPIO 34 (Left) or 35 (Right)
              |
           [10k ohm]
              |
             GND
```

### Power Chain

```
Solar Panel (+) --> TP4056 IN+
Solar Panel (-) --> TP4056 IN-

TP4056 B+ / B- --> 18650 Battery

TP4056 OUT+ --> Rocker Switch --> ESP32 VIN (5V)
                                    Servo VCC (5V)
TP4056 OUT- --> Common GND
```

Servo signal: orange wire to GPIO 12. Red to 5V. Brown to GND.

## Setup

### 1. Arduino IDE Setup

1. Install [Arduino IDE](https://www.arduino.cc/en/software) (2.x recommended)
2. Add ESP32 board manager URL: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Install **esp32 by Espressif Systems** via Boards Manager
4. Select: **ESP32 Dev Module**, Upload Speed: **921600**

### 2. Install Libraries

Install via Library Manager:

- `ESP32Servo` by John K. Bennett
- `Adafruit SSD1306` by Adafruit
- `Adafruit GFX Library` by Adafruit

### 3. Upload

1. Open `Solar_Tracker.ino`
2. Connect ESP32 via USB
3. If upload fails: hold BOOT, press EN, release BOOT
4. Click Upload

### 4. Calibrate LDRs

See [`docs/CALIBRATION.md`](docs/CALIBRATION.md)

## Operation

### 3-Button Navigation

| Button           | Normal Mode          | Menu Mode         |
| ---------------- | -------------------- | ----------------- |
| LEFT (GPIO 4)    | Manual: rotate left  | Scroll up         |
| RIGHT (GPIO 19)  | Manual: rotate right | Scroll down       |
| SELECT (GPIO 18) | Open menu            | Confirm selection |

### Main Menu

- **Auto Track** — Servo tracks sun automatically
- **Manual Control** — LEFT/RIGHT buttons control servo
- **Calibrate LDR** — Read current light as baseline
- **System Info** — Uptime, status, thresholds
- **Deep Sleep** — Enter sleep mode now (storage/transport)
- **Settings** — Adjust LDR thresholds

### Buzzer Tone Patterns

| Event        | Pattern                  |
| ------------ | ------------------------ |
| Startup      | to-to-too                |
| Mode change  | to-to                    |
| Calibrate    | totot                    |
| Manual press | to (click)               |
| Center/Reset | to-to-to                 |
| Page cycle   | to-to                    |
| Night/Sleep  | to-to-to--- (descending) |
| Error        | totototot (rapid)        |

### OLED Display

- **Normal view:** LDR bar graphs, mode indicator, tracking status, animated sun icon
- **Menu view:** Slide-in animation, highlighted selection, scrollable items
- **Boot sequence:** Sun rise animation with loading bar

## Project Structure

```
Solar-Tracker-Embedded/
├── Solar_Tracker.ino      # Main firmware
├── components/             # Component datasheets & specs
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
│   ├── WIRING.md           # Detailed wiring guide
│   ├── PINOUT.md           # Pin reference table
│   ├── SETUP.md            # Step-by-step setup
│   ├── CALIBRATION.md      # LDR calibration process
│   ├── BEST_PRACTICES.md   # Embedded C++ best practices
│   ├── DECISIONS.md        # Engineering decisions log
│   ├── BLE_DESIGN.md       # Bluetooth integration design
│   ├── CONCEPTS.md         # C++ concepts quick reference
│   └── TRACKER.md          # Project progress tracker
├── circut.png              # Circuit diagram
└── README.md               # This file
```

## Notes

- GPIO 34-39 are input-only on ESP32 — no internal pull-up. LDRs use voltage dividers.
- Servo needs external 5V — use TP4056 boost output, not ESP32 5V pin.
- TP4056 has no battery protection — monitor battery voltage, don't drain below 3.0V.
- Solar panel output varies — 1.5W / 250mA is slow charging. Expect longer charge times on cloudy days.

## Compilation Status

```
Flash: 354 KB / 1.3 MB (26%)
RAM:   24 KB / 327 KB  (7%)
Status: Clean compile, zero errors
```

---

Built for Embedded Systems Course — Year 1, Semester 2, BUE
