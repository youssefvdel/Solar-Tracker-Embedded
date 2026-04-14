# Push Button — PBS-110 (Red, Normally Open)

## Overview

Self-resetting (momentary) push button switch, normally open. Closes circuit when pressed.

## Key Specs

| Spec                | Value                      |
| ------------------- | -------------------------- |
| **Type**            | Normally Open (NO)         |
| **Action**          | Momentary (self-resetting) |
| **Thread Diameter** | 7 mm                       |
| **Button Diameter** | 6 mm                       |
| **Terminals Pitch** | 3.5 mm                     |
| **Overall Size**    | 27.5 × 10 mm               |
| **Rating**          | AC 125V/3A or AC 250V/1A   |

## Wiring

```
ESP32 GPIO ────[Internal Pull-Up]──── 3.3V
                     |
                   [Button]
                     |
                    GND
```

- Wire one leg to GPIO, other leg to GND.
- Use `INPUT_PULLUP` in code (internal pull-up resistor).
- Pressed = `LOW`, Released = `HIGH`.

## Pin Assignments (Sun Tracker) — TBD

| Button | GPIO | Function |
| ------ | ---- | -------- |
| BTN 1  | TBD  | TBD      |
| BTN 2  | TBD  | TBD      |
| BTN 3  | TBD  | —        |
| BTN 4  | TBD  | —        |
| BTN 5  | TBD  | —        |
| BTN 6  | TBD  | —        |

## Suggested Functions

Pick what you need (don't have to use all 6):

| Function         | Description                                     |
| ---------------- | ----------------------------------------------- |
| **Mode Toggle**  | Cycle: Auto → Manual → Off                      |
| **Calibrate**    | Trigger LDR calibration (read ambient baseline) |
| **Manual Left**  | Rotate servo left while held                    |
| **Manual Right** | Rotate servo right while held                   |
| **Reset**        | Return to center/starting position              |
| **Buzzer Mute**  | Silence/enable buzzer                           |

## Notes

- **Software debounce** needed (50–100 ms delay after press detection).
- Available GPIOs (not already used): 4, 13, 15, 16, 17, 18, 19, 23, 25, 26, 27
- Avoid GPIO 34–39 (input-only, no pull-up support).
