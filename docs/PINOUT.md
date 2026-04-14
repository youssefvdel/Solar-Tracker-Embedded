# 🔌 Sun Tracker — Pinout Reference

Quick reference for all ESP32 GPIO connections. Keep this open while building the circuit.

---

## ESP32 Pin Assignment Table

| GPIO | Component | Direction | Notes |
|:----:|-----------|-----------|-------|
| **4**  | Button LEFT   | Input (PULLUP)  | Scroll up / Manual rotate left |
| **12** | Servo Signal  | Output (PWM)     | MG996R orange wire |
| **13** | Button RIGHT  | Input (PULLUP)  | Scroll down / Manual rotate right |
| **14** | Buzzer (+)    | Output (Digital) | Active buzzer, fixed 3.9kHz |
| **15** | Button SELECT | Input (PULLUP)  | Confirm / Open menu |
| **21** | OLED SDA      | I2C Data         | Default ESP32 I2C |
| **22** | OLED SCL      | I2C Clock        | Default ESP32 I2C |
| **33** | LDR Left      | Input (ADC)      | Input-only pin, 12-bit |
| **34** | LDR Right     | Input (ADC)      | Input-only pin, 12-bit |

---

## Power Connections (No GPIO)

| Terminal | Connection |
|----------|-----------|
| **3.3V** | OLED VCC, LDR divider top |
| **5V**   | Servo VCC (from TP4056 boost OUT+) |
| **GND**  | Common ground (ALL components) |

---

## Button Wiring

All 3 buttons use the same pattern:

```
ESP32 GPIO ───→ Button leg 1
Button leg 2 ───→ GND
```

`INPUT_PULLUP` enabled in code — pressed = `LOW`, released = `HIGH`.

---

## LDR Voltage Divider

```
3.3V ──[LDR]──┬── ADC Pin (33 or 34)
              │
           [10kΩ]
              │
             GND
```

---

## Servo Wiring

```
ESP32 GPIO 12 ─────→ Servo Orange (Signal)
TP4056 OUT+ (5V) ──→ Servo Red (VCC)
Common GND ─────────→ Servo Brown (GND)
```

⚠️ **Servo VCC from TP4056, NOT ESP32 5V pin.**

---

## OLED Wiring (I2C)

```
OLED VCC ── 3.3V or 5V
OLED GND ── GND
OLED SCL ── GPIO 22
OLED SDA ── GPIO 21
```

---

## Buzzer Wiring

```
Buzzer (+) ── GPIO 14
Buzzer (-) ── GND
```

---

## Free GPIOs (Available for Future Expansion)

| GPIO | Notes |
|------|-------|
| 0, 2, 5 | Safe to use |
| 16, 17, 18, 19, 23, 25, 26, 27 | Safe to use |
| 23 | SPI MOSI (avoid if using SPI) |
| **32** | ADC-capable, safe |
| **35, 36, 39** | Input-only, ADC-capable |

**Avoid:** GPIO 6–11 (flash), 34–39 (input-only, no pull-up).

---

*Last updated: 2026-04-14 | v1.0 firmware*
