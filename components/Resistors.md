# Resistors — 220Ω & 1kΩ

## Overview
Carbon film resistors for current limiting and pull-up/pull-down in the circuit.

## Shopping List Summary
| Value | Quantity | Purpose |
|-------|----------|---------|
| **10kΩ** | 8 pcs (2×4) | LDR voltage dividers (2× 10kΩ for left + right LDR) |
| **220Ω** | 8 pcs (2×4) | TBD — likely LED current limiting or buzzer protection |
| **1kΩ** | 8 pcs (2×4) | TBD — likely button pull-down or signal conditioning |

## 10kΩ Resistors
- Used in **voltage divider** with each LDR.
- One per LDR (2 total needed).
- Connected between ADC pin and GND.

## 220Ω Resistors
Common uses:
- **LED current limiting** (if adding indicator LEDs)
- **Buzzer series resistor** (optional, for current limiting)

## 1kΩ Resistors
Common uses:
- **Pull-down resistors** for buttons (if not using internal pull-up)
- **Signal conditioning** for GPIO inputs

## Notes
- 1/4W power rating — more than enough for signal-level circuits.
- You have 8 of each value — plenty of spares.
- Exact usage depends on final circuit design.
