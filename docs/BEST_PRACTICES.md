# 📘 Embedded Firmware Best Practices

A living reference for writing clean, efficient, and reliable embedded firmware on the ESP32. Written for the Sun Tracker team — use it, share it, improve it.

---

## 📐 Table of Contents

1. [Types & Data](#1-types--data)
2. [Constants & Magic Numbers](#2-constants--magic-numbers)
3. [Timing — Never Use `delay()`](#3-timing--never-use-delay)
4. [Memory Management](#4-memory-management)
5. [Power Management](#5-power-management)
6. [ADC & Sensor Reading](#6-adc--sensor-reading)
7. [State Machines](#7-state-machines)
8. [Button Handling](#8-button-handling)
9. [Coding Standards](#9-coding-standards)
10. [Architecture & Modularity](#10-architecture--modularity)
11. [Debugging & Logging](#11-debugging--logging)
12. [Safety & Reliability](#12-safety--reliability)

---

## 1. Types & Data

### Rule: Always use fixed-width types. Never use bare `int`.

| Type | Size | Use For |
|------|------|---------|
| `uint8_t` | 1 byte | Pin numbers (0–39), loop counters < 256, byte flags, servo values (0–180) |
| `uint16_t` | 2 bytes | ADC readings (0–4095), timing intervals, sensor values |
| `uint32_t` | 4 bytes | `millis()` timestamps, large counters, frequencies |
| `uint64_t` | 8 bytes | Deep sleep timers (microseconds), large totals |
| `int16_t` | 2 bytes | Signed differences (LDR delta: −4095 to +4095) |
| `int32_t` | 4 bytes | Signed large values, temperature ×100 |
| `bool` | 1 byte | True/false flags |

### Why?

```cpp
// ❌ BAD — ambiguous, wastes memory
int pin = 12;       // 4 bytes for a value that fits in 1
int ldr = analogRead(34);  // 4 bytes, but ADC is 12-bit (0-4095)
int diff = left - right;   // What if this goes negative?

// ✅ GOOD — explicit, self-documenting
uint8_t pin = 12;       // 1 byte, clearly a pin number
uint16_t ldr = analogRead(34);  // 2 bytes, fits ADC range
int16_t diff = left - right;    // 2 bytes, handles negative
```

### ESP32 `int` is 4 bytes!

On Arduino Uno, `int` is 2 bytes. On ESP32, `int` is **4 bytes** (same as `long`). Code that works on Uno may waste 2× memory on ESP32. Always be explicit.

---

## 2. Constants & Magic Numbers

### Rule: No magic numbers. Every constant has a named identifier.

```cpp
// ❌ BAD — what does 50 mean?
if (abs(diff) > 50) {
  servo.write(90);
}

// ✅ GOOD — intent is clear
static constexpr uint16_t LDR_THRESHOLD = 50;
static constexpr uint8_t  SERVO_STOP    = 90;

if (abs(diff) > LDR_THRESHOLD) {
  servo.write(SERVO_STOP);
}
```

### Use `static constexpr` over `#define`

```cpp
// ❌ BAD — preprocessor macro, no type safety, no scope
#define PIN_LED 13
#define MAX_TEMP 100

// ✅ GOOD — type-safe, debugger-friendly, respects namespaces
static constexpr uint8_t PIN_LED   = 13;
static constexpr uint8_t MAX_TEMP  = 100;
```

**Why `constexpr` wins:**
- Compiler knows the type → catches mismatches at compile time
- Shows up in debugger (macros don't)
- Respects C++ scope rules (can be inside `namespace` or `class`)
- No accidental text substitution bugs

---

## 3. Timing — Never Use `delay()`

### Rule: Use `millis()` for all timing. `delay()` blocks everything.

```cpp
// ❌ BAD — CPU is frozen for 1000ms. Buttons? Servo? OLED? All dead.
void loop() {
  readSensor();
  delay(1000);
  updateDisplay();
  delay(500);
}

// ✅ GOOD — everything runs concurrently
static constexpr uint32_t INTERVAL_SENSOR = 1000;
static constexpr uint32_t INTERVAL_DISPLAY = 500;
uint32_t lastSensor = 0;
uint32_t lastDisplay = 0;

void loop() {
  uint32_t now = millis();

  if (now - lastSensor >= INTERVAL_SENSOR) {
    lastSensor = now;
    readSensor();
  }

  if (now - lastDisplay >= INTERVAL_DISPLAY) {
    lastDisplay = now;
    updateDisplay();
  }
}
```

### How `millis()` works:
- Returns milliseconds since boot (`uint32_t`, max ~49.7 days)
- Wraps around to 0 after overflow — but `now - last >= interval` still works correctly due to unsigned arithmetic
- Resolution: 1 ms

### When `delay()` IS acceptable:
- Very short waits (< 200ms) in `setup()` or one-time operations
- Debounce confirmation after edge detection
- Letting hardware settle (e.g., OLED before `esp_deep_sleep_start()`)

**Never use `delay()` in the main loop for periodic tasks.**

---

## 4. Memory Management

### Rule: No `malloc`, no `new`, no `delete`. Use static allocation.

### Why heap allocation is dangerous in embedded:
| Problem | Description |
|---------|-------------|
| **Fragmentation** | Repeated alloc/free creates gaps → out-of-memory despite free RAM |
| **No recovery** | `malloc` returns NULL on failure — easy to forget to check |
| **Leaks** | Forgotten `free` = permanent memory loss (no GC in C++) |
| **Non-deterministic** | Allocation time varies — bad for real-time systems |

### ESP32 SRAM Layout:
| Region | Size | Purpose |
|--------|------|---------|
| Static RAM | ~290 KB | Global/static variables |
| Stack | ~8 KB per task | Local variables, function calls |
| Heap | ~200 KB | Dynamic allocation (avoid!) |

### Best practices:

```cpp
// ❌ BAD — heap allocation
void process() {
  int* buffer = new int[256];  // Heap! Fragmentation risk.
  // ...
  delete[] buffer;  // Easy to forget
}

// ✅ GOOD — stack allocation (automatic cleanup)
void process() {
  uint16_t buffer[256];  // Stack — freed when function returns
  // ...
}

// ✅ GOOD — static allocation (lifetime = entire program)
static uint16_t sensorBuffer[256];  // Global, no allocation overhead

void process() {
  // Use sensorBuffer directly
}
```

### Use `static` for persistent state:

```cpp
// Debounce state that must survive between loop() calls
struct ButtonState {
  bool     pressed;
  bool     lastRaw;
  uint32_t lastChange;
};
static ButtonState btnStates[6];  // Initialized to zero, persists forever
```

---

## 5. Power Management

### ESP32 Sleep Modes

| Mode | Current | CPU | RAM | Wi-Fi | Wake Sources |
|------|---------|-----|-----|-------|-------------|
| **Active** | 20–240 mA | ✅ Running | ✅ Retained | ✅ On | — |
| **Modem Sleep** | 20–68 mA | ✅ Running | ✅ Retained | 🔄 Cycled | — |
| **Light Sleep** | ~0.8 mA | ⏸ Paused | ✅ Retained | ❌ Off | GPIO, Timer, UART |
| **Deep Sleep** | 6–150 µA | ❌ Off | ❌ Lost | ❌ Off | RTC Timer, RTC GPIO, Touch |
| **Hibernation** | ~5 µA | ❌ Off | ❌ Lost | ❌ Off | RTC Timer only |

**Power reduction:** Deep sleep uses **~1/1000th** the power of active mode.

### Deep Sleep Best Practices

```cpp
// ✅ Timer wake: check conditions periodically
esp_sleep_enable_timer_wakeup(30 * 60ULL * 1000000ULL); // 30 minutes

// ✅ GPIO wake: button press wakes immediately
esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, LOW);

// ✅ Multiple GPIO wake (more efficient than ext0)
esp_sleep_enable_ext1_wakeup(BIT(GPIO_NUM_4) | BIT(GPIO_NUM_13),
                              ESP_EXT1_WAKEUP_ALL_LOW);

// ⚠️ Before sleeping — clean up!
servo.detach();           // Stop PWM output
digitalWrite(LED, LOW);   // Turn off peripherals
delay(500);               // Let hardware settle
esp_deep_sleep_start();   // Never returns — full reset on wake
```

### Wake Sources Comparison

| Method | Current | Best For |
|--------|---------|----------|
| Timer only | 6–10 µA | Periodic sensor checks |
| Ext0 (1 GPIO) | 50–100 µA | Single button wake |
| Ext1 (multi GPIO) | ~10 µA | Multiple buttons |
| Touch | ~30 µA | Capacitive touch |
| ULP coprocessor | ~100 µA (1% duty) | Smart wake on sensor threshold |

### Power Budget Example (Sun Tracker)

| State | Current | Hours/Day | Energy/Day |
|-------|---------|-----------|------------|
| Active (tracking) | 150 mA | 8 | 1200 mAh |
| Deep sleep (night) | 50 µA | 16 | 0.8 mAh |
| **Total** | | | **~1201 mAh** |

With a 2500 mAh 18650 → **~2 days autonomy** without sun.

---

## 6. ADC & Sensor Reading

### ESP32 ADC Quirks

| Issue | Cause | Solution |
|-------|-------|----------|
| **Noise** | WiFi interference, shared ground | Oversample + hardware filter |
| **Non-linearity** | ESP32 ADC is inherently non-linear | Calibrate with known voltages |
| **Input-only pins** | GPIO 34–39 have no pull-up/pull-down | Use voltage divider |
| **Attenuation** | Default range 0–1.1V (not 0–3.3V) | Set `adcAttachPin()` or use voltage divider |

### Hardware: Voltage Divider + Capacitor

```
3.3V ──[LDR]──┬── ADC Pin ──[0.1µF]── GND
              │
           [10kΩ]
              │
             GND
```

The **0.1µF capacitor** acts as a low-pass filter, smoothing high-frequency noise.

### Software: Oversampling

```cpp
// Average 16 samples → reduces noise by √16 = 4×
uint16_t readLDR(uint8_t pin) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 16; i++) {
    sum += analogRead(pin);
  }
  return static_cast<uint16_t>(sum / 16);
}
```

### Why oversampling works:
- Random noise averages toward zero
- 4 samples → ~2× improvement, 16 samples → ~4×, 64 samples → ~8×
- Diminishing returns — 16 is the sweet spot for most applications

### Best Practice Checklist:
- [ ] Use voltage divider with appropriate resistor value
- [ ] Add 0.1µF capacitor at ADC input
- [ ] Oversample (16× recommended)
- [ ] Calibrate for your environment
- [ ] Use GPIO 32–39 for analog (input-only, no pull-up needed)
- [ ] Avoid WiFi during critical ADC reads (or use `WiFi.setSleep(true)`)

---

## 7. State Machines

### Rule: Model system behavior as a finite state machine (FSM).

Every embedded system is a state machine. Make it **explicit**, not implicit.

```cpp
// ❌ BAD — implicit states scattered as boolean flags
bool isTracking = true;
bool isNight = false;
bool isCalibrating = false;

// ✅ GOOD — explicit enum, one source of truth
enum SystemMode  { MODE_AUTO, MODE_MANUAL, MODE_OFF };
enum SystemState { STATE_ACTIVE, STATE_NIGHT_SLEEP };

SystemMode  currentMode  = MODE_AUTO;
SystemState currentState = STATE_ACTIVE;
```

### FSM Transition Table

Document allowed transitions:

```
MODE_AUTO ──[btn]──→ MODE_MANUAL ──[btn]──→ MODE_OFF ──[btn]──→ MODE_AUTO
   │                                           │
   └──[night]──→ STATE_NIGHT_SLEEP ──[day]──→ (wakes to MODE_AUTO)
```

### Implementation Pattern:

```cpp
void handleModeTransition() {
  switch (currentMode) {
    case MODE_AUTO:
      trackSun();
      if (isNight()) enterSleep();
      break;

    case MODE_MANUAL:
      handleManualInput();
      break;

    case MODE_OFF:
      sunServo.write(SERVO_STOP);
      break;
  }
}
```

### Benefits:
- All states visible at a glance
- Impossible states are impossible to reach
- Easy to add logging/debugging per state
- Easy to document and explain to teammates

---

## 8. Button Handling

### Rule: Always debounce. Always poll with `millis()`.

### Mechanical Bouncing:
When you press a button, the contacts physically bounce for 10–50ms, creating dozens of rapid HIGH/LOW transitions. Without debouncing, one press = 20+ events.

### Software Debounce Pattern:

```cpp
bool readButton(uint8_t pin, uint8_t btnIndex) {
  bool raw = digitalRead(pin) == LOW;
  uint32_t now = millis();

  if (raw != btnStates[btnIndex].lastRaw) {
    btnStates[btnIndex].lastChange = now;  // Reset timer on edge
  }
  btnStates[btnIndex].lastRaw = raw;

  // Only accept if stable LOW for ≥ debounce window
  if (raw && (now - btnStates[btnIndex].lastChange >= DEBOUNCE_MS)) {
    if (!btnStates[btnIndex].pressed) {
      btnStates[btnIndex].pressed = true;
      return true;  // Single event: just pressed
    }
  } else if (!raw) {
    btnStates[btnIndex].pressed = false;  // Reset on release
  }
  return false;
}
```

### Why NOT interrupts for buttons?

| Approach | Pros | Cons |
|----------|------|------|
| **Polling + debounce** | Simple, reliable, no ISR complexity | Tiny CPU overhead (negligible on 240MHz) |
| **`attachInterrupt()`** | Zero polling overhead | ISRs can't use `delay()`, `Serial`, or `malloc`; debouncing in ISR is complex; ESP32 GPIO interrupt limitations |

**Verdict:** Poll every 20ms. On a 240 MHz CPU, checking 6 buttons takes ~1 µs. The complexity of interrupts is not worth it.

**Exception:** Use GPIO interrupts for **wake-from-deep-sleep** — that's what they're designed for.

---

## 9. Coding Standards

### Naming Conventions

| Element | Style | Example |
|---------|-------|---------|
| Macros / `constexpr` | `SCREAMING_SNAKE_CASE` | `PIN_LED`, `MAX_TEMP` |
| Types / Enums | `PascalCase` | `SystemMode`, `ButtonState` |
| Functions | `camelCase` | `readLDR()`, `updateDisplay()` |
| Variables | `camelCase` | `ldrLeft`, `lastSensor` |
| Private members | `_camelCase` | `_calibrated` |
| Pin constants | `PIN_` prefix | `PIN_SERVO`, `PIN_LDR_LEFT` |

### Function Design

```cpp
// ✅ One function, one responsibility
uint16_t readLDR(uint8_t pin);      // Reads and averages
void updateDisplay();                // Updates OLED only
void trackSun();                     // Compares LDRs, moves servo

// ❌ Don't mix concerns in one function
void doEverything() {
  readLDR();
  moveServo();
  updateDisplay();
  checkButtons();
  // Hard to test, hard to debug, hard to reuse
}
```

### Comments: Explain WHY, not WHAT

```cpp
// ❌ BAD — restates the code
servo.write(90);  // Write 90 to servo

// ✅ GOOD — explains the reasoning
servo.write(SERVO_STOP);  // Center = stop for 360° continuous rotation servo
```

### No `goto`, no recursion (unbounded), no variable-length arrays

```cpp
// ❌ BAD
int buffer[n];  // VLA — not standard C++, stack overflow risk

// ✅ GOOD
static constexpr uint8_t MAX_BUFFER = 256;
uint16_t buffer[MAX_BUFFER];  // Compile-time known size
```

---

## 10. Architecture & Modularity

### Rule: Separate concerns. Each module has one job.

### Recommended File Structure:

```
Sun_Tracker/
├── Sun_Tracker.ino      # Main: setup(), loop(), state machine
├── src/
│   ├── sensors.cpp      # LDR reading, calibration
│   ├── sensors.h
│   ├── actuator.cpp     # Servo control
│   ├── actuator.h
│   ├── display.cpp      # OLED rendering
│   ├── display.h
│   ├── input.cpp        # Button handling, debounce
│   ├── input.h
│   └── power.cpp        # Deep sleep, night detection
│       └── power.h
```

### Separation of Concerns:

| Module | Responsible For |
|--------|----------------|
| `sensors` | Reading LDRs, averaging, calibration data |
| `actuator` | Servo attach/detach, speed control, stop |
| `display` | OLED init, layout, rendering |
| `input` | Button debouncing, event detection |
| `power` | Sleep modes, wake sources, battery monitoring |
| `main` | State machine, event routing, timing |

### Why this matters:
- **Testable** — you can test `readLDR()` independently of the servo
- **Reusable** — `sensors.cpp` works in any project with LDRs
- **Readable** — finding button code? Go to `input.cpp`
- **Team-friendly** — one person works on display, another on power, no merge conflicts

---

## 11. Debugging & Logging

### Serial at 115200, not 9600

```cpp
Serial.begin(115200);  // ✅ ESP32 standard — fast, no bottleneck
// Serial.begin(9600); // ❌ Arduino legacy — painfully slow on ESP32
```

### Structured Logging

```cpp
// ✅ Clear, parseable format
Serial.printf("L: %4u | R: %4u | Diff: %+d | Mode: %s\n",
              ldrLeft, ldrRight,
              static_cast<int16_t>(ldrLeft - ldrRight),
              modeStr);

// ❌ Unstructured
Serial.print("left is ");
Serial.print(ldrLeft);
Serial.print(" and right is ");
Serial.println(ldrRight);
```

### Log Levels (for larger projects):

```cpp
#define LOG_ERROR(fmt, ...)   Serial.printf("[E] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)    Serial.printf("[W] " fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)    Serial.printf("[I] " fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)   Serial.printf("[D] " fmt "\n", ##__VA_ARGS__)
```

### Watchdog Timer (for production):

```cpp
// ESP32 has hardware watchdog — use it to catch freezes
#include <esp_task_wdt.h>

void setup() {
  esp_task_wdt_init(10, true);  // 10-second timeout, panic on trigger
  esp_task_wdt_add(NULL);       // Add current task
}

void loop() {
  esp_task_wdt_reset();  // Reset watchdog each loop
  // If loop freezes, watchdog triggers restart
}
```

---

## 12. Safety & Reliability

### Initialize Everything

```cpp
// ✅ Explicit initialization
void setup() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);  // Start silent
  sunServo.write(SERVO_STOP);     // Start stationary
}
```

### Handle Peripheral Failures

```cpp
// OLED might fail to init — don't crash silently
if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
  Serial.println("ERROR: OLED init failed!");
  // Option 1: Halt and wait for fix
  while (true);
  // Option 2: Continue without display (degraded mode)
  // displayAvailable = false;
}
```

### Never Trust External Input

```cpp
// ✅ Validate before using
uint16_t ldr = readLDR(PIN_LDR_LEFT);
if (ldr > 4095) ldr = 4095;  // ADC should never exceed this, but just in case
```

### Document Assumptions

```cpp
// ASSUMPTION: LDRs are mounted at equal height on opposite sides.
// If mounted asymmetrically, calibrate with offset:
//   int diff = (left + LEFT_OFFSET) - (right + RIGHT_OFFSET);
```

### Power-On Self Test (POST)

```cpp
void runSelfTest() {
  Serial.println("Running POST...");

  // Check OLED
  bool oledOk = display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  Serial.printf("OLED: %s\n", oledOk ? "OK" : "FAIL");

  // Check servo responds
  sunServo.attach(PIN_SERVO);
  sunServo.write(SERVO_STOP);
  delay(100);
  Serial.println("Servo: OK");

  // Check LDRs
  uint16_t l = readLDR(PIN_LDR_LEFT);
  uint16_t r = readLDR(PIN_LDR_RIGHT);
  Serial.printf("LDR: L=%u R=%u %s\n", l, r,
                (l > 0 && r > 0) ? "OK" : "FAIL");

  Serial.println("POST complete.");
}
```

---

## 📚 Quick Reference Card

```
┌─────────────────────────────────────────────────────────┐
│  TYPES                                                  │
│  uint8_t   → pins, flags, small counters (0-255)        │
│  uint16_t  → ADC values (0-4095), intervals             │
│  uint32_t  → millis(), timestamps, large counters       │
│  int16_t   → signed differences                         │
│  bool      → true/false flags                           │
│                                                         │
│  TIMING                                                 │
│  millis()  → non-blocking timing                        │
│  delay()   → ONLY in setup() or <200ms one-time waits   │
│                                                         │
│  CONSTANTS                                              │
│  static constexpr uint8_t PIN = 12;  ← not #define      │
│                                                         │
│  POWER                                                  │
│  Active: 150mA | Deep Sleep: 50µA (3000× less!)         │
│  esp_deep_sleep_start() → never returns                 │
│                                                         │
│  ADC                                                    │
│  Oversample 16× → 4× noise reduction                    │
│  Add 0.1µF capacitor at ADC pin                         │
│                                                         │
│  BUTTONS                                                │
│  Poll every 20ms + 50ms debounce → no interrupts needed │
│                                                         │
│  MEMORY                                                 │
│  No malloc/new/delete. Use stack or static.             │
│                                                         │
│  STATE MACHINES                                         │
│  enum for states. switch for transitions.               │
└─────────────────────────────────────────────────────────┘
```

---

*Last updated: 2026-04-14 | For the Sun Tracker team — BUE Embedded Systems*
