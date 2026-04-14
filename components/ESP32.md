# ESP-32S 30-Pin Development Board

## Overview

Compact ESP32 SoC board with WiFi, Bluetooth, and CP2102 USB-to-Serial chip for programming.

## Key Specs

| Spec                  | Value                                                           |
| --------------------- | --------------------------------------------------------------- |
| **Chipset**           | ESP-WROOM-32 (ESP32)                                            |
| **Processor**         | Dual-core Xtensa LX6, up to 240 MHz                             |
| **Flash**             | 4 MB                                                            |
| **Wireless**          | WiFi 802.11 b/g/n, Bluetooth v4.2 + BLE                         |
| **Operating Voltage** | 3.3V                                                            |
| **Power Input**       | 5V via Micro USB                                                |
| **GPIO**              | 30-pin access (UART, SPI, I²C, I²S, 12-bit ADC, 8-bit DAC, PWM) |
| **Board Size**        | ~51 × 25 mm                                                     |
| **Dev Support**       | Arduino IDE, ESP-IDF, MicroPython, PlatformIO                   |

## Pin Assignments (Sun Tracker)

| GPIO | Component  | Direction    | Notes          |
| ---- | ---------- | ------------ | -------------- |
| 12   | Servo 360° | Output (PWM) | MG996R signal  |
| 33   | Left LDR   | Input (ADC)  | Input-only pin |
| 34   | Right LDR  | Input (ADC)  | Input-only pin |
| 14   | Buzzer     | Output (PWM) |                |
| 21   | OLED SDA   | I2C Data     | Default I2C    |
| 22   | OLED SCL   | I2C Clock    | Default I2C    |
| TBD  | Buttons    | Input        | To be assigned |

## Onboard Features

- Reset & Boot buttons
- 3.3V voltage regulator
- Power LED

## Notes

- GPIO 34–39 are **input-only** (no internal pull-up/pull-down).
- Default I2C pins are 21 (SDA) / 22 (SCL).
