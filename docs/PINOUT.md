# Sun Tracker — Pinout Reference

Quick reference for all ESP32 GPIO connections. Keep this open while building the circuit.

**Current firmware wiring (v1.1)** — limiter pins were swapped from v1.0 to match the physical copper bar layout.

---

## ESP32 Pin Assignment Table

| GPIO | Component | Direction | Notes |
|:----:|-----------|-----------|-------|
| **4**  | Button LEFT    | Input (PULLUP)  | Scroll up / Manual rotate left |
| **12** | Servo Signal   | Output (PWM)     | MG996R orange wire |
| **13** | Limiter RIGHT  | Input (PULLUP)  | Copper bar — stop when panel reaches right end |
| **14** | Buzzer (+)     | Output (Digital) | Active buzzer, fixed 3.9kHz |
| **15** | Limiter LEFT   | Input (PULLUP)  | Copper bar — stop when panel reaches left end |
| **18** | Button SELECT  | Input (PULLUP)  | Confirm / Open menu |
| **19** | Button RIGHT   | Input (PULLUP)  | Scroll down / Manual rotate right |
| **21** | OLED SDA       | I2C Data         | Default ESP32 I2C |
| **22** | OLED SCL       | I2C Clock        | Default ESP32 I2C |
| **34** | LDR Left       | Input (ADC)      | Input-only pin, 12-bit |
| **35** | LDR Right      | Input (ADC)      | Input-only pin, 12-bit |
| **36** | Battery Sense  | Input (ADC)      | Voltage divider for battery % |

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

## Limiter (Copper Bar) Wiring

The two copper bars act as momentary switches — one side connects to the ESP32 GPIO, the other side connects to **GND**. When the rotating arm touches the bar, it pulls the GPIO to `LOW`.

```
ESP32 GPIO 13 ───→ Copper Bar RIGHT ───→ GND
ESP32 GPIO 15 ───→ Copper Bar LEFT  ───→ GND
```

**Why the pins were swapped (v1.1):** The original pinout had GPIO 13 as the left limiter and GPIO 15 as the right limiter, but the physical servo rotation direction was inverted relative to the expected "left/right" mapping. Swapping the limiter pins in code fixed the direction without rewiring the copper bars.

---

## LDR Voltage Divider

```
3.3V ──[LDR]──┬── ADC Pin (34 or 35)
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

**Servo VCC from TP4056, NOT ESP32 5V pin.**

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

## Battery Voltage Sensing

```
Battery (+) ──[10kΩ]──┬── GPIO 36
                      │
                   [10kΩ]
                      │
                     GND
```

This gives a 2:1 voltage divider, so a 4.2V full battery reads as ~2.1V at the ADC. The code scales this back to get battery percentage.

---

## Free GPIOs (Available for Future Expansion)

| GPIO | Notes |
|------|-------|
| 0, 2, 5 | Safe to use |
| 16, 17, 23, 25, 26, 27 | Safe to use |
| 23 | SPI MOSI (avoid if using SPI) |
| **32** | ADC-capable, safe |
| **39** | Input-only, ADC-capable |

**Avoid:** GPIO 6–11 (flash), 34–39 (input-only, no pull-up — but 34/35/36 are already used for sensors).

---

*Last updated: 2026-04-23 | v1.1 firmware — limiter pins swapped to match physical layout*
