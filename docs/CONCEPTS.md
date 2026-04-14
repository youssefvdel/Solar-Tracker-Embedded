# 💡 Embedded C++ Quick Concepts

Answers to the most common *"wait, what does this actually do?"* moments. Written for the Sun Tracker team — keep it open while reading code.

---

## 1. Function Pointers: `void (*action)(void)`

### What it is
A variable that stores the **address of a function**, not a number or text.

### Why we use it
So each menu item carries its own "click handler". No ugly `if/else` chains.

### Visual
```
Menu Item                Behind the Scenes
┌──────────────┐         ┌──────────────┐
│ "Auto Track" │─────→   │ action       │
└──────────────┘         │ 0x400A1234   │  ← points to a function
                         └──────┬───────┘
                                │
                                ▼
                         void actionAutoTrack() {
                           currentMode = MODE_AUTO;
                         }
```

### Syntax Breakdown
```cpp
void (*action)(void)
 │       │         │
 │       │         └─ Takes NO arguments
 │       └─ Variable name ("action")
 └─ Returns NOTHING

Read as: "action is a pointer to a function that takes nothing and returns nothing"
```

### In Our Code
```cpp
// Menu array pairs text + function
{"Auto Track", actionAutoTrack},
{"Calibrate",  actionCalibrate},

// When you press SELECT:
currentMenu[menuIndex].action(); // Calls whatever function is stored there
```

**✅ Key Takeaway:** Function pointers let data drive behavior. Clean, scalable, zero `if/else` soup.

---

## 2. Address Operator: `&Wire`

### What it is
`&` gets the **memory address** (reference) of an object. It passes a pointer, not a copy.

### Why we use it
The OLED library needs **direct access** to the ESP32's I2C hardware controller. Passing a copy would break I2C communication.

### Visual
```
Without &:  Wire → [COPY] → Library (works with duplicate, loses hardware link) ❌
With &:     Wire → [ADDRESS] → Library (talks to real hardware directly) ✅

Think of it like:
  Wire     = your actual phone
  &Wire    = your phone number (someone can call the real device)
```

### In Our Code
```cpp
Adafruit_SSD1306 display(128, 64, &Wire, -1);
//                            ^^^^^
// Passes the actual TwoWire I2C instance, not a copy
```

**✅ Key Takeaway:** `&` = "use the real thing, don't copy it". Required when libraries need hardware access.

---

## 3. `static_cast<type>(value)`

### What it is
A **compile-time type conversion** that safely forces a value into a specific data type.

### Why we use it
Prevents silent bugs like unsigned wrap-around or arithmetic overflow.

### Visual: The Wrap Bug
```
uint16_t (unsigned):  0 ─────────────── 65535
                        ↑ -500 wraps to 65036 = WRONG ❌

int16_t (signed):    -32768 ────── 0 ────── 32767
                                  ↑ -500 = CORRECT ✅

Code:
uint16_t left = 1500, right = 2000;
int16_t diff = static_cast<int16_t>(left - right); // diff = -500 ✓
```

### Visual: Overflow Prevention
```
32-bit max:  4,294,967,295
64-bit max:  18,446,744,073,709,551,615

30 × 60 × 1,000,000 = 1.8B  (fits in 32-bit, barely)
100 × 60 × 1,000,000 = 6B   (OVERFLOW in 32-bit!) ❌

static_cast<uint64_t>(30) → upgrades to 64-bit BEFORE multiplying
Now: 64-bit × 64-bit × 64-bit = always safe ✅
```

### In Our Code
```cpp
// LDR difference (prevent unsigned wrap)
int16_t diff = static_cast<int16_t>(ldrLeft - ldrRight);

// Deep sleep timer (prevent overflow)
esp_sleep_enable_timer_wakeup(
    static_cast<uint64_t>(SLEEP_CHECK_INTERVAL) * 60ULL * 1000000ULL
);
```

**✅ Key Takeaway:** `static_cast` tells the compiler "I know what I'm doing, convert this safely". Safer than C-style `(int)x`.

---

## 4. `map()` + `constrain()` Pipeline

### What it is
Two math functions chained together to **clamp** and **scale** values.

### Why we use it
Sensor readings (0–4095) need to fit screen pixels or servo ranges. Raw values would break the UI.

### Visual Pipeline
```
Raw LDR Diff:  -4095 ═══════════════ 0 ═══════════════ +4095
                          │         │         │
                     constrain   center   constrain
                          │         │         │
                         -200       0        +200
                          │         │         │
                         map       map       map
                          │         │         │
                         -15        0        +15  ← pixels
                          │         │         │
              Sun icon moves ←─── center ───→
```

### How It Works
```cpp
// Step 1: Constrain (clamp to safe range)
constrain(diff, -200, 200)
// -500 → -200
// 10   → 10
// 300  → 200

// Step 2: Map (scale to target range)
map(clamped, -200, 200, -15, 15)
// -200 → -15 pixels (max left)
// 0    → 0 pixels (center)
// 200  → +15 pixels (max right)
```

### In Our Code
```cpp
int16_t diff = static_cast<int16_t>(ldrLeft - ldrRight);
sunX += map(constrain(diff, -200, 200), -200, 200, -15, 15);
```

**✅ Key Takeaway:** `constrain()` protects bounds, `map()` scales to UI space. Always constrain FIRST, then map.

---

## 📋 TL;DR Cheat Card

| Concept | Syntax | Purpose | Team Rule |
|---------|--------|---------|-----------|
| **Function Pointer** | `void (*fn)(void)` | Store functions in variables/arrays | Use for menu actions, callbacks |
| **Address Operator** | `&object` | Pass reference, not copy | Required for hardware libs (Wire, SPI, etc.) |
| **Static Cast** | `static_cast<T>(x)` | Safe type conversion | Never use C-style `(T)x` |
| **Constrain + Map** | `map(constrain(x, a, b), c, d)` | Clamp then scale | Always constrain BEFORE map |

---

*Keep this open while reviewing code. If something else looks weird, add it here. — Luna*
