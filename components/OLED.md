# OLED Display — 0.96" I2C SSD1306 (128×64)

## Overview

Compact monochrome OLED display with I2C interface, driven by the SSD1306 controller chip.

## Key Specs

| Spec                  | Value              |
| --------------------- | ------------------ |
| **Size**              | 0.96 inch          |
| **Type**              | OLED               |
| **Driver**            | SSD1306            |
| **Resolution**        | 128 × 64 pixels    |
| **Interface**         | I2C (4-pin)        |
| **Display Area**      | 21.744 × 10.864 mm |
| **Module Size**       | 27.3 × 27.8 mm     |
| **Viewing Angle**     | > 160°             |
| **Working Voltage**   | 3.3V / 5V          |
| **Power Dissipation** | ~0.06W (ultra-low) |
| **Temp Range**        | -20°C to 60°C      |

## Wiring

| Pin | Label | ESP32 Connection |
| --- | ----- | ---------------- |
| 1   | GND   | GND              |
| 2   | VCC   | 3.3V or 5V       |
| 3   | SCL   | **GPIO 22**      |
| 4   | SDA   | **GPIO 21**      |

## Library

- **Arduino:** `Adafruit_SSD1306` + `Adafruit_GFX`
- **I2C Address:** Typically `0x3C`

## Suggested Display Content

- Left & Right LDR values (raw ADC)
- Servo direction indicator (← / → / ■)
- Current mode (Auto / Manual / Calibrate)
- Alert status (buzzer active indicator)

## Notes

- Only 2 IO pins needed (I2C bus).
- 3.3V and 5V logic compatible — no level shifter required.
- Industrial temp range suitable for outdoor use.
