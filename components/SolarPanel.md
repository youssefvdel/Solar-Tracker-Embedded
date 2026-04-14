# Mini Solar Panel — 1.5W 6V / 250mA

## Overview
Small photovoltaic panel for harvesting solar energy to charge the 18650 battery via the TP4056 module.

## Key Specs
| Spec | Value |
|------|-------|
| **Power** | 1.5W |
| **Voltage** | 6V (open circuit) |
| **Current** | 250mA (max) |
| **Size** | 165 × 65 mm |
| **Type** | Polycrystalline / Monocrystalline (check panel) |

## Wiring
```
Solar Panel (+) ──→ TP4056 IN+ (or B+ pad)
Solar Panel (-) ──→ TP4056 IN- (or B- pad)
```

## Notes
- 1.5W is relatively low power — charging will be slow (TP4056 defaults to 1A charge, but solar panel only provides ~250mA max).
- Works with TP4056 charger module — charges battery during daylight.
- Position panel facing the sun for maximum output.
- In this Sun Tracker project, the panel could be **mounted on the tracker itself** — as the servo rotates the platform toward the sun, the panel also optimizes its angle for maximum power generation.
- Output varies with sunlight intensity, angle, and weather.
