# Rocker Switch — SW2 ON/OFF (2-Pin)

## Overview
Simple single-pole single-throw (SPST) rocker switch for main power control.

## Key Specs
| Spec | Value |
|------|-------|
| **Type** | SPST (Single Pole, Single Throw) |
| **Action** | ON / OFF (maintained position) |
| **Terminals** | 2-pin copper terminals |
| **Rating** | Up to 6A / 250V AC |
| **Color** | Black |

## Wiring (Power Line)
```
Battery OUT+ ──→ Switch Terminal 1
Switch Terminal 2 ──→ ESP32 VIN / System 5V rail
```

## Purpose
- **Main power switch** — cuts power to entire system when OFF.
- Saves battery when not in use.
- Placed between TP4056 boost output and the rest of the circuit.

## Notes
- Simple mechanical switch — no GPIO connection needed.
- Mount in an accessible location on the project enclosure.
