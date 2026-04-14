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
| **Overall Size**    | 27.5 x 10 mm               |
| **Rating**          | AC 125V/3A or AC 250V/1A   |

## Wiring

```
ESP32 GPIO ---[Internal Pull-Up]--- 3.3V
                    |
                  [Button]
                    |
                   GND
```

- Wire one leg to GPIO, other leg to GND.
- Use `INPUT_PULLUP` in code (internal pull-up resistor).
- Pressed = `LOW`, Released = `HIGH`.

## Pin Assignments (Sun Tracker)

| Button | GPIO | Function                          |
| ------ | ---- | --------------------------------- |
| LEFT   | 4    | Scroll up / Manual rotate left    |
| RIGHT  | 13   | Scroll down / Manual rotate right |
| SELECT | 15   | Confirm / Open menu               |

## Notes

- Software debounce needed (50-100 ms delay after press detection).
- 3 buttons used out of 6 available. 3 spares kept.
