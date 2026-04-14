# MG996R Servo Motor — 360° Continuous Rotation

## Overview

High-torque digital servo with metal gears, modified for continuous rotation (360° ± 60° from center).

## Key Specs

| Spec                  | Value                             |
| --------------------- | --------------------------------- |
| **Stall Torque**      | 9.4 kgf·cm (4.8V), 11 kgf·cm (6V) |
| **Operating Speed**   | 0.17s/60° (4.8V), 0.14s/60° (6V)  |
| **Rotation Angle**    | 360° (±60° from center)           |
| **Operating Voltage** | 4.8V – 6V                         |
| **Running Current**   | 500 mA                            |
| **Stall Current**     | 2.5 A (6V)                        |
| **Dead Band Width**   | 5 µs                              |
| **Weight**            | 55 g                              |
| **Dimensions**        | 40 × 20 × 37 mm                   |
| **Gears**             | Metal                             |
| **Bearings**          | Double ball bearing               |
| **Temp Range**        | 0°C – 55°C                        |

## Wiring

| Wire Color | Connection                             |
| ---------- | -------------------------------------- |
| Orange     | PWM Signal → **GPIO 12**               |
| Red        | VCC → 5V (external supply recommended) |
| Brown      | GND                                    |

## Control (Continuous Rotation)

| `write(value)` | Behavior                        |
| -------------- | ------------------------------- |
| 90             | Stop (center)                   |
| 0              | Full speed CW                   |
| 180            | Full speed CCW                  |
| 0–89           | Speed increases toward full CW  |
| 91–180         | Speed increases toward full CCW |

## Notes

- **Not angle-based** — values control speed and direction, not position.
- May stall or jitter if ESP32 5V pin can't source enough current. External 5V supply recommended.
- Dead band of 5 µs means small PWM changes near 90 may not move the servo.
