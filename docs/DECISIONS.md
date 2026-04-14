# Engineering Decisions & Best Practices

Research-backed decisions for the Sun Tracker project. Each point verified, accepted, or rejected with reasoning.

---

## Accepted (Will Implement)

### 1. Use `millis()` Instead of `delay()` — NON-BLOCKING TIMING

**Status:** ACCEPTED

**Why:**

- `delay()` blocks the entire CPU — nothing else runs (buttons, OLED updates, servo control)
- `millis()` checks elapsed time without blocking, allowing multitasking
- Critical for: button polling + servo control + OLED refresh + LDR reading all happening concurrently

**Implementation:**

```cpp
// Instead of:
delay(1000);

// Use:
const unsigned long INTERVAL = 1000;
unsigned long lastUpdate = 0;

void loop() {
  if (millis() - lastUpdate >= INTERVAL) {
    lastUpdate = millis();
    // Do the thing
  }
}
```

**Applied to:** LDR reading interval, OLED refresh rate, button debounce timer, servo update rate.

---

### 2. Serial Monitor at 115200 Baud — NOT 9600

**Status:** ACCEPTED (Already Implemented)

**Why:**

- ESP32 UART hardware supports up to 5 Mbps easily
- 9600 baud is from old Arduino Uno era (16 MHz ATmega328P)
- ESP32 runs at 240 MHz — 115200 is the standard, barely scratches its bandwidth
- Higher baud = faster serial debug output, less blocking time in `Serial.print()`

**Already set in code:**

```cpp
Serial.begin(115200);
```

---

### 3. LDR Oversampling — Average 16 Readings

**Status:** ACCEPTED

**Why:**

- ESP32 ADC is noisy (especially with WiFi enabled)
- Single readings can fluctuate by ±50-100 counts
- Averaging 16 samples reduces noise by ~4× (√16 = 4)
- Takes ~1-2ms total — negligible overhead

**Implementation:**

```cpp
int readLDR(uint8_t pin) {
  long sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogRead(pin);
  }
  return sum / 16;
}
```

---

### 4. Button Debounce

**Status:** ACCEPTED

**Why:**

- Mechanical buttons "bounce" — rapid ON/OFF transitions for 10-50ms when pressed
- Without debounce, one press registers as multiple presses
- Software debounce with `millis()` is cleaner than hardware RC filters

**Implementation:**

```cpp
bool readButtonDebounced(uint8_t pin, unsigned long debounceMs = 50) {
  static unsigned long lastPress = 0;
  if (digitalRead(pin) == LOW) {
    if (millis() - lastPress > debounceMs) {
      lastPress = millis();
      return true; // Pressed
    }
  }
  return false;
}
```

---

### 5. Deep Sleep for Power Saving

**Status:** ACCEPTED

**Why:**

- This is a **solar-powered** system — every µA matters
- ESP32 active current: ~80-250mA
- ESP32 deep sleep current: ~10-150µA (RTC only)
- That's a **1000× reduction** in power consumption

**Strategy:**

- **Day mode:** Active tracking while solar panel is generating power
- **Night mode:** Deep sleep when LDR readings drop below threshold (no sun)
- Wake on: Timer (check every 30 min) or GPIO interrupt (manual button override)

**Implementation:**

```cpp
// Enter deep sleep
esp_sleep_enable_timer_wakeup(30 * 60 * 1000000); // 30 minutes in µs
esp_deep_sleep_start();
```

**Power Budget:**
| State | Current | Duration | Daily Energy |
|-------|---------|----------|-------------|
| Active (tracking) | ~150 mA | 8 hrs (daylight) | 1200 mAh |
| Deep sleep (night) | ~50 µA | 16 hrs | 0.8 mAh |
| **Total** | | | **~1201 mAh/day** |

With a 18650 battery (~2500-3500 mAh), this gives **2-3 days of autonomy** without sun.

---

## Rejected / Not Applicable (With Reasoning)

### 6. Use SPI Instead of I2C for OLED

**Status:** REJECTED — Keep I2C

**Why:**

- SPI IS faster than I2C (SPI: up to 40 MHz, I2C: 400 kHz standard)
- Our OLED module is **I2C only** (4-pin: GND, VCC, SCL, SDA)
- SPI variant requires 7 pins (GND, VCC, CS, DC, CLK, MOSI, RST)
- For a 128×64 text display updating every 1-2 seconds, I2C speed is MORE than enough
- I2C uses only 2 GPIO pins vs SPI's 4+ — we need those pins for buttons
- **Verdict:** I2C is the right choice for this project. SPI speed advantage is irrelevant here.

---

### 7. Use `ledcAttach()` at 25kHz for Servo PWM

**Status:** REJECTED — Servos Need 50Hz, Not 25kHz

**Why:**

- The note about `analogWrite()` defaulting to 1kHz (audible whine) is correct for motors/LEDs
- `ledcAttach()` is the right ESP32-native way to set custom PWM frequencies
- BUT servos don't use high-frequency PWM — they use **50Hz (20ms period)** with pulse widths of 1-2ms
- 25kHz would NOT work with a standard servo — the control signal would be meaningless
- The `ESP32Servo` library already handles this correctly under the hood (uses `ledcAttach()` at 50Hz)

**Servo PWM Explained:**

```
50Hz = 20ms period
├── Pulse width 1.0ms → Full CCW (or full speed one direction for 360°)
├── Pulse width 1.5ms → Center / Stop
└── Pulse width 2.0ms → Full CW (or full speed opposite direction)
```

**Conclusion:** The `ESP32Servo` library already does the right thing. No manual `ledcAttach()` needed.

**When `ledcAttach()` WOULD be useful:**

- Driving passive buzzers at specific frequencies (we have an active buzzer — doesn't need it)
- PWM LED dimming at inaudible frequencies (>20kHz)
- DC motor speed control

---

### 8. Use `attachInterrupt()` for Buttons Instead of Polling

**Status:** REJECTED — Polling with Debounce is Better Here

**Why:**

- Interrupts DO save CPU cycles vs polling
- ESP32 GPIO interrupts have limitations:
  - Only works on specific pins (not all GPIOs support it)
  - ISRs run in interrupt context — can't use `delay()`, `Serial.print()`, or most Arduino functions
  - Need `volatile` variables and careful synchronization
- For 6 buttons, the CPU overhead of polling every 10-50ms is **negligible** on a 240 MHz dual-core CPU
- Interrupts add complexity (edge cases, race conditions, debouncing in ISR is tricky)
- **Deep sleep + GPIO wake** is a much better power-saving strategy than button interrupts during active mode

**When interrupts WOULD make sense:**

- A single critical "emergency stop" button
- Wake-from-deep-sleep trigger (we'll use this!)
- High-frequency signal capture (encoder, sensor pulse)

**Our approach:** Poll buttons every 20ms with debounce. Use deep sleep for real power savings.

---

## Future Consideration

### 9. Bluetooth Integration

**Status:** DEFERRED — Good idea, needs scope definition

**ESP32 has Bluetooth (BLE + Classic) — but what for?**

**Possible use cases:**

| Use Case              | Description                                                     | Complexity | Priority |
| --------------------- | --------------------------------------------------------------- | ---------- | -------- |
| **Remote monitoring** | Phone app shows real-time LDR values, battery %, servo position | Medium     | High     |
| **Remote control**    | Override tracking, switch modes from phone                      | Medium     | Medium   |
| **Configuration**     | Adjust thresholds, calibrate remotely (no need to re-flash)     | Low        | High     |
| **Data logging**      | Stream LDR data to phone for analysis/graphing                  | Medium     | Medium   |
| **Firmware OTA**      | Update firmware wirelessly via BLE                              | High       | Low      |

**Recommended approach (if we add BLE):**

1. BLE Server with custom service
2. Characteristics for:
   - LDR Left value (read)
   - LDR Right value (read)
   - Servo position (read)
   - System mode (read/write)
   - Battery level (read)
3. Use a free BLE app like "nRF Connect" or build a simple one

**Recommendation:** Add this in **Phase 2** after the core tracking works. It's a great showcase feature but adds complexity.

---

## Summary

| #   | Decision                        | Status          | Impact   |
| --- | ------------------------------- | --------------- | -------- |
| 1   | `millis()` non-blocking timing  | Accepted        | Critical |
| 2   | Serial at 115200 baud           | Accepted (done) | Minor    |
| 3   | LDR oversampling (16× avg)      | Accepted        | Moderate |
| 4   | Button debounce                 | Accepted        | Critical |
| 5   | Deep sleep for power saving     | Accepted        | Critical |
| 6   | SPI instead of I2C for OLED     | Rejected        | —        |
| 7   | `ledcAttach()` 25kHz for servo  | Rejected        | —        |
| 8   | `attachInterrupt()` for buttons | Rejected        | —        |
| 9   | Bluetooth integration           | Deferred        | Future   |
