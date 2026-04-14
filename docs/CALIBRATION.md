# 📐 LDR Calibration Guide

Calibrating the LDR sensors is essential for accurate sun tracking. The threshold values determine when the servo moves and in which direction.

---

## Why Calibrate?

LDR readings vary based on:
- **Ambient light levels** (sunny vs cloudy, indoor vs outdoor)
- **Resistor tolerance** (10kΩ resistors aren't exactly 10kΩ)
- **LDR positioning** (angle, distance from panel edges)
- **Time of day** (sun intensity changes)

Without calibration, the servo may:
- Oscillate back and forth (hunting)
- Never stop moving
- Ignore small light differences
- Be too sensitive to shadows

---

## Calibration Process

### Step 1 — Read Raw Values

Upload this calibration sketch temporarily (or add to main code):

```cpp
void setup() {
  Serial.begin(115200);
  pinMode(33, INPUT);  // Left LDR
  pinMode(34, INPUT);  // Right LDR
}

void loop() {
  int left  = analogRead(33);
  int right = analogRead(34);
  int diff  = left - right;
  
  Serial.printf("Left: %4d | Right: %4d | Diff: %+4d\n", left, right, diff);
  delay(500);
}
```

### Step 2 — Test Conditions

Place the tracker in each condition and record readings:

| Condition | Left LDR | Right LDR | Difference |
|-----------|----------|-----------|------------|
| **Equal light** (both shaded equally) | ___ | ___ | ___ |
| **Left brighter** (shine light on left) | ___ | ___ | ___ |
| **Right brighter** (shine light on right) | ___ | ___ | ___ |
| **Both in full sun** | ___ | ___ | ___ |
| **Both in shade** | ___ | ___ | ___ |

### Step 3 — Determine Threshold

Calculate the threshold values:

```
THRESHOLD = average difference when one side is noticeably brighter
DEADZONE  = max difference when both sides should be "equal"
```

**Recommended starting values:**
```cpp
#define LDR_THRESHOLD   50   // Move servo if difference > 50
#define LDR_DEADZONE    20   // Don't move if difference < 20
```

### Step 4 — Tune the Values

Test and adjust:

| Symptom | Fix |
|---------|-----|
| Servo oscillates / hunts | ↑ Increase threshold |
| Servo too slow to respond | ↓ Decrease threshold |
| Servo jitters near center | ↑ Increase deadzone |
| Servo ignores small light changes | ↓ Decrease deadzone |

### Step 5 — Outdoor Calibration

Once values work indoors, test outdoors:

1. Place tracker in direct sunlight
2. Observe LDR readings on Serial Monitor
3. Note the max/min values in real conditions
4. Adjust threshold/deadzone accordingly

Outdoor readings will be **much higher** than indoor (closer to 4095 in full sun).

---

## Calibration Tips

### LDR Placement
- Mount LDRs at **equal height** on opposite sides
- Angle them slightly outward (~15°) for better directional sensing
- Shield them from light coming from behind the panel

### Reduce Noise
- Take **multiple readings** and average them:
  ```cpp
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(PIN_LDR_LEFT);
    delay(5);
  }
  int ldrLeft = sum / 10;
  ```
- Use a **capacitor** (0.1µF) across the 10kΩ resistor to filter noise

### Adaptive Threshold
For varying conditions, consider dynamic thresholding:
```cpp
// Threshold scales with overall light level
int avgLight = (ldrLeft + ldrRight) / 2;
int dynamicThreshold = map(avgLight, 0, 4095, 100, 30);
```

---

## Expected Reading Ranges

| Condition | LDR Value (ADC 0-4095) |
|-----------|------------------------|
| Complete darkness | 0–50 |
| Indoor room light | 500–1500 |
| Near window (daylight) | 1500–3000 |
| Direct sunlight | 3000–4095 |

---

## Save Calibrated Values

Once calibrated, update these in `Sun_Tracker.ino`:

```cpp
#define LDR_THRESHOLD   50   // ← Your calibrated value
#define LDR_DEADZONE    20   // ← Your calibrated value
```
