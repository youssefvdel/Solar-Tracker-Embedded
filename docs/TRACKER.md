# Solar Tracker — Project Progress Tracker

**Last Updated:** 2026-04-14
**Team:** BUE Embedded Systems — Year 1, Semester 2
**Repo:** https://github.com/youssefvdel/Solar-Tracker-Embedded

---

## Overview

| Phase                              | Status   | Progress |
| ---------------------------------- | -------- | -------- |
| Phase 1 — Planning & Documentation | Complete | 100%     |
| Phase 2 — Core Firmware            | Complete | 100%     |
| Phase 3 — Assembly & Testing       | Pending  | 0%       |
| Phase 4 — Power & Deployment       | Pending  | 0%       |
| Phase 5 — Polish & Delivery        | Pending  | 0%       |

---

## Phase 1 — Planning & Documentation (COMPLETE)

- [x] Define project scope and requirements
- [x] Create shopping list and order components
- [x] Research components (servo, LDR, OLED, buzzer, TP4056, solar panel)
- [x] Document all components in `components/` folder
- [x] Create wiring diagrams and pin assignments
- [x] Research ESP32 best practices
- [x] Write `docs/BEST_PRACTICES.md` for team reference
- [x] Create `docs/WIRING.md` with complete wiring guide
- [x] Create `docs/PINOUT.md` pin reference table
- [x] Create `docs/SETUP.md` with Arduino IDE and library setup
- [x] Create `docs/CALIBRATION.md` for LDR calibration
- [x] Create `docs/DECISIONS.md` documenting engineering choices
- [x] Create `docs/BLE_DESIGN.md` Bluetooth integration design
- [x] Create `docs/CONCEPTS.md` C++ concepts quick reference
- [x] Set up project README
- [x] Create GitHub repository

---

## Phase 2 — Core Firmware (COMPLETE)

### 2.1 — Project Structure

- [x] Create `Solar_Tracker.ino` with full project
- [x] Include all libraries (ESP32Servo, Adafruit_SSD1306, Adafruit_GFX, Wire)
- [x] Define all pin constants with `static constexpr uint8_t`
- [x] Define all config constants (thresholds, timing, servo values)
- [x] Set up state machine enums (SystemMode, SystemState)
- [x] Implement non-blocking timing with `millis()`
- [x] Implement LDR oversampling function (16x average)
- [x] Implement button debounce with `millis()`-based tracking
- [x] Implement deep sleep entry with timer + GPIO wake
- [x] Implement night detection logic
- [x] Implement sun tracking algorithm (LDR comparison to servo movement)
- [x] Implement OLED display with animated UI (boot animation, pulsing sun, bar graphs)
- [x] Implement 3-button menu navigation (LEFT/RIGHT/SELECT)
- [x] Implement active buzzer rhythmic tone patterns
- [x] Implement startup sequence (boot animation, servo init, tone)
- [x] Use fixed-width types throughout (uint8_t, uint16_t, uint32_t)

### 2.2 — Code Quality

- [x] No magic numbers — all constants named
- [x] No `delay()` in main loop (only in setup/boot animation)
- [x] Proper `static_cast` for type conversions
- [x] `printf` format specifiers match variable types
- [x] Comments explain WHY, not WHAT

### 2.3 — Compilation

- [x] Clean compile via Arduino IDE
- [x] Clean compile via `arduino-cli`
- [x] Flash usage: 354 KB / 1.3 MB (26%)
- [x] RAM usage: 24 KB / 327 KB (7%)

### 2.4 — Pending Firmware Tasks

- [ ] LDR calibration routine — auto-calibrate thresholds on button press
- [ ] Smooth servo ramp — gradual speed changes instead of instant
- [ ] BLE remote monitoring — phone app shows LDR values, battery, mode
- [ ] Watchdog timer for production reliability

---

## Phase 3 — Assembly & Testing (PENDING)

### 3.1 — Breadboard Prototype

- [ ] Build complete circuit on breadboard
- [ ] Verify all GND connections are common
- [ ] Set TP4056 boost output to 5.0V (multimeter)
- [ ] Test each component individually before integration
- [ ] ESP32 boots and Serial works
- [ ] OLED displays boot animation
- [ ] Servo moves to stop position
- [ ] LDR readings appear on Serial Monitor
- [ ] Buttons respond (Serial debug)
- [ ] Buzzer plays tone patterns

### 3.2 — Integration Testing

- [ ] Full system power-on test
- [ ] LDR readings match expected ranges (dark vs bright)
- [ ] Servo responds to LDR difference (auto mode)
- [ ] Manual mode buttons control servo
- [ ] Menu navigation works (scroll, select, back)
- [ ] Buzzer tone patterns work for all events
- [ ] Night detection triggers deep sleep
- [ ] Timer wake works (set short interval for testing)
- [ ] Button wake works from deep sleep

### 3.3 — Outdoor Testing

- [ ] Mount LDRs on tracker platform
- [ ] Test in direct sunlight
- [ ] Verify tracking direction (left brighter to rotate right)
- [ ] Verify stop behavior (equal light to centered)
- [ ] Adjust LDR threshold values if needed
- [ ] Test in cloudy/partial conditions
- [ ] Test sunset behavior (night detection)

---

## Phase 4 — Power & Deployment (PENDING)

### 4.1 — Power System

- [ ] Connect 18650 battery to TP4056
- [ ] Connect solar panel to TP4056
- [ ] Verify charging LED behavior
- [ ] Measure power consumption in each mode:
  - [ ] Active (tracking)
  - [ ] OLED on
  - [ ] Servo moving
  - [ ] Deep sleep
- [ ] Calculate expected battery life
- [ ] Test solar charging rate

### 4.2 — Enclosure & Mounting

- [ ] Design/3D print tracker base
- [ ] Mount ESP32 securely
- [ ] Mount servo with platform
- [ ] Position LDRs at equal height, opposite sides
- [ ] Mount OLED in visible location
- [ ] Mount buttons in accessible location
- [ ] Route wires cleanly
- [ ] Weather considerations (if outdoor)

### 4.3 — Field Deployment

- [ ] Full outdoor test with solar power
- [ ] Monitor battery level over full day/night cycle
- [ ] Verify autonomous operation (no manual intervention needed)
- [ ] Test reliability over multiple days

---

## Phase 5 — Polish & Delivery (PENDING)

### 5.1 — Code Polish

- [ ] Remove all debug Serial prints (or gate behind `#ifdef DEBUG`)
- [ ] Add final code comments and documentation
- [ ] Code review against `docs/BEST_PRACTICES.md`
- [ ] Optimize any remaining inefficiencies

### 5.2 — Presentation Prep

- [ ] Prepare demo video or live demo
- [ ] Document architecture diagram
- [ ] Write project summary/report
- [ ] Prepare component cost breakdown
- [ ] Document lessons learned

### 5.3 — Stretch Goals (If Time Permits)

- [ ] BLE remote monitoring
- [ ] Web server interface (ESP32 soft AP)
- [ ] Data logging to SD card
- [ ] Dual-axis tracking (add second servo for elevation)
- [ ] MPPT solar charging optimization

---

## Bug Tracker

| ID  | Status | Description | Severity | Assigned |
| --- | ------ | ----------- | -------- | -------- |
| --  | --     | --          | --       | --       |

_No bugs logged yet — we haven't tested on hardware._

---

## Notes & Decisions Log

| Date       | Decision / Note                                                            |
| ---------- | -------------------------------------------------------------------------- |
| 2026-04-14 | Use `ESP32Servo` library (not `ESP32Servo360`) — MG996R has no hall sensor |
| 2026-04-14 | Fixed-width types throughout — no bare `int`                               |
| 2026-04-14 | `millis()` timing — no `delay()` in main loop                              |
| 2026-04-14 | Deep sleep for night — timer + GPIO wake                                   |
| 2026-04-14 | I2C OLED (not SPI) — module is I2C-only, 4-pin                             |
| 2026-04-14 | Button polling with debounce — not interrupts                              |
| 2026-04-14 | LDR oversampling 16x for noise reduction                                   |
| 2026-04-14 | 3-button menu (LEFT/RIGHT/SELECT) instead of 6 dedicated buttons           |
| 2026-04-14 | Active buzzer with rhythmic patterns (not melodies — fixed pitch)          |
| 2026-04-14 | MODE_STANDBY replaces old MODE_OFF (Deep Sleep is a real action)           |
| 2026-04-14 | Repo created: github.com/youssefvdel/Solar-Tracker-Embedded                |
