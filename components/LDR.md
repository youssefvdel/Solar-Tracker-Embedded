# LDR Photoresistor — GL12528 (12mm)

## Overview

Cadmium sulfide (CdS) light-dependent resistor for ambient light sensing. Used in pairs (left & right) for sun tracking.

## Key Specs

| Spec         | Value             |
| ------------ | ----------------- |
| **Model**    | GL12528           |
| **Diameter** | 12 mm             |
| **Type**     | CDS Photoresistor |

## Wiring (Voltage Divider)

```
VCC (3.3V)
  |
 [LDR]
  |
  +---→ ADC Pin (GPIO 33 or 34)
  |
 [10kΩ Resistor]
  |
 GND
```

## Pin Assignments (Sun Tracker)

| Sensor | ESP32 GPIO | Type                 |
| ------ | ---------- | -------------------- |
| Left   | 33         | ADC (12-bit, 0–4095) |
| Right  | 34         | ADC (12-bit, 0–4095) |

## How It Works

- Resistance **decreases** as light intensity **increases**.
- In a voltage divider, more light → higher ADC reading.
- Compare left vs right LDR values to determine which side is brighter.
- Servo rotates to balance both LDR readings (equal light = tracker aligned).

## Notes

- GPIO 34 & 33 are **input-only** on ESP32.
- Calibration recommended — ambient light varies by environment.
- Typical dark resistance: ~1 MΩ, light resistance: ~10 kΩ.
- A 10kΩ fixed resistor works well for the voltage divider.
