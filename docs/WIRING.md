# Wiring Guide

Complete wiring reference for the Sun Tracker project. All connections use the ESP32 as the central controller.

---

## 1. ESP32 Pin Map

| GPIO | Component | Type | Notes |
|------|-----------|------|-------|
| 12 | Servo Signal | PWM Out | MG996R orange wire |
| 33 | LDR Left | ADC In | Input-only pin |
| 34 | LDR Right | ADC In | Input-only pin |
| 14 | Buzzer | Digital Out | Active buzzer |
| 21 | OLED SDA | I2C | Default ESP32 I2C |
| 22 | OLED SCL | I2C | Default ESP32 I2C |
| 4 | Button 1 | Digital In | Pull-up |
| 13 | Button 2 | Digital In | Pull-up |
| 15 | Button 3 | Digital In | Pull-up |
| 16 | Button 4 | Digital In | Pull-up |
| 17 | Button 5 | Digital In | Pull-up |
| 18 | Button 6 | Digital In | Pull-up |

---

## 2. LDR Voltage Dividers (×2)

Each LDR forms a voltage divider with a 10kΩ resistor.

```
        3.3V
         │
       ┌─┴─┐
       │LDR│  ← Light dependent resistance
       └─┬─┘
         ├──────→ GPIO 33 (Left) or 34 (Right)
       ┌─┴─┐
       │10k│  ← Fixed resistor
       └─┬─┘
         │
        GND
```

### Physical Wiring
1. Insert LDR into breadboard
2. Connect one LDR leg to **3.3V** rail
3. Connect other LDR leg to **GPIO 33** (Left) or **34** (Right)
4. From that same leg, connect a **10kΩ resistor** to **GND**

### How It Works
| Condition | LDR Resistance | ADC Reading |
|-----------|---------------|-------------|
| Dark | ~1 MΩ | Low (~0–200) |
| Bright | ~10 kΩ | High (~2000–4095) |

---

## 3. Servo Motor (MG996R 360°)

```
ESP32 GPIO 12 ─────→ Servo Orange (Signal/PWM)
5V Rail (TP4056 OUT+) ──→ Servo Red (VCC)
Common GND ──────────→ Servo Brown (GND)
```

### Important
- **Do NOT power the servo from ESP32 5V pin** — it can draw up to 2.5A at stall
- Use the **TP4056 boost converter output** (set to 5V) for servo VCC
- Ensure **common GND** between ESP32, servo, and power supply

---

## 4. OLED Display (SSD1306 I2C)

```
OLED Pin 1 (GND)  ──→ ESP32 GND
OLED Pin 2 (VCC)  ──→ ESP32 3.3V (or 5V)
OLED Pin 3 (SCL)  ──→ ESP32 GPIO 22
OLED Pin 4 (SDA)  ──→ ESP32 GPIO 21
```

### Notes
- 4-pin I2C module — only needs 2 signal wires
- Default I2C address: `0x3C`
- Pull-up resistors usually built into the module

---

## 5. Buzzer (Active Piezo)

```
Buzzer (+) ──→ ESP32 GPIO 14
Buzzer (-) ──→ ESP32 GND
```

### Notes
- Active buzzer — built-in oscillator at ~3.9 kHz
- Just apply HIGH/LOW to sound/silence
- Current draw: ~10mA (safe for ESP32 GPIO)

---

## 6. Push Buttons (×6)

Each button uses the same wiring pattern:

```
ESP32 GPIO ──→ Button leg 1
Button leg 2 ──→ GND
```

Code enables internal pull-up resistors (`INPUT_PULLUP`), so:
- **Pressed** = `LOW` (0V)
- **Released** = `HIGH` (3.3V)

### Button Pin Assignments
| Button | GPIO | Suggested Function |
|--------|------|-------------------|
| BTN 1 | 4 | Mode Toggle |
| BTN 2 | 13 | Calibrate |
| BTN 3 | 15 | Manual Left |
| BTN 4 | 16 | Manual Right |
| BTN 5 | 17 | Reset / Center |
| BTN 6 | 18 | Buzzer Mute |

---

## 7. Power System

### TP4056 Charger + Boost Converter

```
Solar Panel (+) ──────────→ TP4056 IN+ (or B+ pad)
Solar Panel (-) ──────────→ TP4056 IN- (or B- pad)

TP4056 B+ / B- pads ─────→ 18650 Battery (+) / (-)

TP4056 OUT+ ──→ Rocker Switch ──→ ESP32 VIN (5V pin)
                                     Servo VCC
                                     OLED VCC (optional)

TP4056 OUT- ─────────────→ Common GND
```

### Setup Steps
1. **Set boost voltage:** Use a multimeter on OUT+/OUT- and adjust the trim pot clockwise until you read **5.0V**
2. **Connect battery:** 18650 to B+/B- pads (polarity matters!)
3. **Connect solar panel:** to IN+/IN- (or charge via USB first)
4. **Connect rocker switch:** between OUT+ and ESP32 VIN
5. **Connect loads:** ESP32 VIN, Servo VCC to the switched 5V rail

### LED Indicators on TP4056
| LED | Meaning |
|-----|---------|
| Red | Battery charging |
| Blue/Green | Battery fully charged (4.2V) |

---

## 8. Complete Breadboard Layout

```
┌─────────────────────────────────────────────────────────┐
│  Breadboard                                              │
│                                                          │
│  [Solar Panel]                                           │
│      │                                                   │
│  [TP4056] ── OUT+ ──[Switch]── 5V Rail ──┐              │
│      │                                    │              │
│     GND ────────────────────────────── GND Rail          │
│                                                          │
│  5V Rail ──→ Servo VCC (Red)                            │
│  GND Rail ─→ Servo GND (Brown)                          │
│                                                          │
│  ESP32                                                   │
│  ├── GPIO 12 ──→ Servo Signal (Orange)                  │
│  ├── GPIO 33 ──→ LDR Left divider midpoint              │
│  ├── GPIO 34 ──→ LDR Right divider midpoint             │
│  ├── GPIO 14 ──→ Buzzer (+)                             │
│  ├── GPIO 21 ──→ OLED SDA                               │
│  ├── GPIO 22 ──→ OLED SCL                               │
│  ├── GPIO 4,13,15,16,17,18 ──→ Buttons                  │
│  └── 3.3V ──→ LDR divider top, OLED VCC                 │
│                                                          │
│  LDR Left:  3.3V ──[LDR]──┬── GPIO 33 ──[10kΩ]── GND   │
│  LDR Right: 3.3V ──[LDR]──┬── GPIO 34 ──[10kΩ]── GND   │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

---

## 9. Checklist Before Powering On

- [ ] All GND connections are common (ESP32, servo, OLED, buzzer, buttons, TP4056)
- [ ] LDR voltage dividers use 10kΩ resistors (not 220Ω or 1kΩ)
- [ ] Servo VCC comes from TP4056 boost output (NOT ESP32 5V pin)
- [ ] TP4056 boost voltage set to ~5.0V (verify with multimeter)
- [ ] OLED SDA/SCL connected to correct pins (21/22)
- [ ] Buttons wired to GND (not 3.3V) — pull-up is in code
- [ ] 18650 battery polarity correct on TP4056 B+/B-
- [ ] Rocker switch between TP4056 OUT+ and ESP32 VIN
- [ ] No loose wires or shorts on breadboard
