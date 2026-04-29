# Solar Tracker — Workflow

## Overview
ESP32 solar tracker with dual LDR sensors, 360° continuous servo, copper limit switches, and OLED display.

---

## Boot / Setup
1. Init serial, pins, OLED
2. Attach servo, write STOP (90°)
3. Read initial LDR values
4. Play startup tone

---

## Main Loop (runs every ~20ms)
1. **Buzzer update** — play tone pattern if active
2. **Servo stop check** — if pulse timer expired, write SERVO_STOP
3. **Read limiters** — debounced (3 consecutive LOW reads)
4. **Read LDRs** — every 200ms, 128-sample ADC average per sensor
5. **Check buttons** — every 20ms with 50ms debounce
6. **Auto tracking** — if mode = AUTO
7. **Update display** — every 100ms

---

## Auto Mode — NORMAL State

### trackingLogic():
1. **Read LDR difference** (left - right)
2. **Centered?** (abs(diff) < 200)
   - YES → stop servo, return
3. **At left limiter AND LDR wants left?**
   - YES → increment stuck counter
   - stuck >= 5? → enter WRAP_RIGHT mode
   - NO → stop servo, return
4. **At right limiter AND LDR wants right?**
   - YES → increment stuck counter
   - stuck >= 5? → enter WRAP_LEFT mode
   - NO → stop servo, return
5. **Direction changed AND below hysteresis?** (abs(diff) < 80)
   - YES → stop servo, return (prevent oscillation)
6. **Pulse servo** — one 15ms pulse toward brighter side
   - Write SERVO_LEFT (180°) or SERVO_RIGHT (0°)
   - Set timer to stop after SERVO_ON (15ms)

### Escape Steps (after wrap completes):
- 5 forced pulses away from limiter bar
- Uses same pulse mechanism as normal tracking
- Returns to normal tracking when done

---

## Auto Mode — WRAP State

- **WRAP_LEFT**: continuous slow left rotation (135°)
  - Until left limiter hit
  - Then stop, set 5 escape steps right, return to NORMAL

- **WRAP_RIGHT**: continuous slow right rotation (45°)
  - Until right limiter hit
  - Then stop, set 5 escape steps left, return to NORMAL

---

## Manual Mode

- **LEFT button held** → pulse left (180°) every 200ms
- **RIGHT button held** → pulse right (0°) every 200ms
- **SELECT button** → open menu
- Limiters still active (cannot pulse into a triggered limit)

---

## Menu (3-button navigation)

| Button | Function |
|--------|----------|
| LEFT | Scroll up |
| RIGHT | Scroll down |
| SELECT | Confirm |

### Menu Items:
1. **Auto** → switch to AUTO mode
2. **Manual** → switch to MANUAL mode
3. **Sleep** → enter deep sleep

---

## Sleep Mode

1. Play sleep tone, show "SLEEP" on OLED
2. Detach servo, turn off buzzer
3. Enter deep sleep:
   - Timer wake: every 30 seconds
   - Button wake: SELECT button press
4. **On timer wake:**
   - Read LDRs immediately
   - If both still dark → silent sleep (no OLED/buzzer)
   - If light detected → full boot, resume tracking

---

## Key Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| SERVO_STOP | 90 | Stop position |
| SERVO_LEFT | 180 | Full speed left |
| SERVO_RIGHT | 0 | Full speed right |
| SERVO_WRAP_LEFT | 135 | Slow wrap speed left |
| SERVO_WRAP_RIGHT | 45 | Slow wrap speed right |
| SERVO_ON | 15ms | Pulse duration |
| LDR_DEADZONE | 200 | Ignore diff below this |
| LDR_HYSTERESIS | 80 | Need this to reverse |
| STUCK_THRESHOLD | 5 | Cycles before wrap (~1s) |
| TIME_LDR | 200ms | LDR read interval |

---

## States Diagram

```
[BOOT] → setup() → [NORMAL]

[NORMAL] → centered → stop
         → need move → pulse
         → at limit + stuck → [WRAP]

[WRAP] → hit opposite limit → escape steps → [NORMAL]

[NORMAL/MANUAL] → SELECT → [MENU]

[MENU] → Auto/Manual → [NORMAL]/[MANUAL]
       → Sleep → [SLEEP]

[SLEEP] → timer wake → check LDRs → dark? → [SLEEP]
                                → light? → [BOOT]
        → button wake → [BOOT]
```
