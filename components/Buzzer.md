# Piezo Buzzer — 3–24V Active Alarm

## Overview

Self-contained piezo buzzer with built-in drive circuit. Produces continuous 95 dB tone when powered.

## Key Specs

| Spec                  | Value                     |
| --------------------- | ------------------------- |
| **Type**              | Piezoelectric (active)    |
| **Diameter**          | 22 mm                     |
| **Height**            | 10 mm                     |
| **Rated Voltage**     | 12V DC                    |
| **Operating Voltage** | 3–24V DC                  |
| **Max Current**       | 10 mA                     |
| **Frequency**         | 3900 ± 500 Hz             |
| **Sound Pressure**    | 95 dB                     |
| **Mounting**          | 2 holes, 30 mm spacing    |
| **Drive Method**      | Built-in circuit (active) |

## Wiring

| Connection | ESP32       |
| ---------- | ----------- |
| Signal (+) | **GPIO 14** |
| GND (-)    | GND         |

## Control

- **Active buzzer** (built-in oscillator): digital `HIGH` = beep, `LOW` = off.
- No PWM needed — just turn it on/off.
- Can use `digitalWrite(14, HIGH)` to sound, `digitalWrite(14, LOW)` to silence.

## Notes

- Can run directly from ESP32 3.3V GPIO (low current draw: 10 mA max).
- Frequency is fixed at ~3.9 kHz — cannot change tone.
- Use for alerts: limits reached, calibration complete, error states.
