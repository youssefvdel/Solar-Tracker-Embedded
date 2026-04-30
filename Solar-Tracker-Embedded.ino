/**
 * ============================================================================
 * Solar Tracker Embedded
 * ============================================================================
 *
 * Dual-axis solar tracker firmware for ESP32 with 360° continuous servo,
 * copper bar limit switches, OLED display, and Bluetooth (SPP) remote control.
 *
 * Modes:
 *   AUTO   - tracks brightest light source using LDR pair
 *   MANUAL - direct servo control via physical buttons or Bluetooth
 *   SLEEP  - light sleep with periodic LDR checks and button wake
 *   WRAP   - internal; unwinds from stuck-limit condition
 *
 * Board   : ESP32 Dev Module (DOIT / NodeMCU-32S)
 * Servo   : 360° continuous rotation
 * Display : SSD1306 128x64 I2C OLED
 *
 * Pinout:
 *   GPIO 34  LDR Left   (ADC1_CH6)
 *   GPIO 35  LDR Right  (ADC1_CH7)
 *   GPIO 36  Battery    (ADC1_CH0, via 10k+10k divider)
 *   GPIO 15  Limit Left  (copper bar, INPUT_PULLUP, LOW = touched)
 *   GPIO 13  Limit Right
 *   GPIO  4  Button Left
 *   GPIO 19  Button Right
 *   GPIO 18  Button Select
 *   GPIO 12  Servo Signal
 *   GPIO 14  Buzzer
 *   GPIO 21  OLED SDA
 *   GPIO 22  OLED SCL
 *
 * Bluetooth Commands (115200 baud, newline-terminated):
 *   A/AUTO, M/MANUAL, S/SLEEP  - mode switch
 *   L/LEFT, R/RIGHT            - manual pulse (MANUAL mode only)
 *   ?/STATUS                   - request telemetry
 *
 * Compile: arduino-cli compile --fqbn esp32:esp32:esp32 .
 * Upload : arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 .
 * ============================================================================
 */

#include <BluetoothSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

// ============================================================================
// PIN DEFINITIONS — all tunable, uint8_t for Arduino pin API
// ============================================================================

// ── OLED I2C ──
static constexpr uint8_t  OLED_SDA = 21;
static constexpr uint8_t  OLED_SCL = 22;
static constexpr int16_t  SCREEN_W = 128;
static constexpr int16_t  SCREEN_H = 64;
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

// ── Servo (360° continuous) ──
static constexpr uint8_t  PIN_SERVO = 12;
Servo servo;

// ── LDR photoresistors (ADC, no pull-up needed) ──
static constexpr uint8_t  PIN_LDR_L = 34;
static constexpr uint8_t  PIN_LDR_R = 35;

// ── Copper-bar limit switches (INPUT_PULLUP, LOW = contact) ──
static constexpr uint8_t  PIN_LIM_L = 15;
static constexpr uint8_t  PIN_LIM_R = 13;

// ── Tactile buttons (INPUT_PULLUP, LOW = pressed) ──
static constexpr uint8_t  PIN_BTN_L = 4;
static constexpr uint8_t  PIN_BTN_R = 19;
static constexpr uint8_t  PIN_BTN_S = 18;

// ── Battery voltage divider (10k + 10k → GPIO 36) ──
static constexpr uint8_t  PIN_BATT = 36;

// ── Piezo buzzer ──
static constexpr uint8_t  PIN_BUZZ = 14;

// ============================================================================
// TUNABLE CONSTANTS — change behaviour without touching logic
// ============================================================================

static constexpr int   LDR_THRESH        = 100;     // LDR diff to trigger tracking
static constexpr int   IDLE_MS           = 30000;   // AUTO → SLEEP after this idle
static constexpr int   SLEEP_CHECK_MS    = 30000;   // wake interval to poll LDRs
static constexpr int   PULSE_MS          = 20;      // servo pulse width per step
static constexpr int   PULSE_WAIT_MS     = 0;       // delay between pulses (0 = no wait)
static constexpr int   MENU_DEBOUNCE_MS  = 200;     // menu button debounce
static constexpr int   BTN_DEBOUNCE_MS   = 50;      // edge-detect debounce window
static constexpr int   WRAP_ESCAPE_PULSES = 5;      // back-off pulses after unwrap
static constexpr int   WRAP_ESCAPE_MS    = 10;      // each escape pulse width
static constexpr int   LDR_SAMPLES       = 1024;    // oversampling count per read
static constexpr int   BT_TELEM_MS       = 1000;    // Bluetooth telemetry interval

// Battery calibration — adjust for your voltage divider and cell chemistry
static constexpr float BATT_DIVIDER = 2.30f;        // divider ratio (R1+R2)/R2
static constexpr float BATT_V_MAX   = 4.10f;        // full-charge voltage
static constexpr float BATT_V_MIN   = 3.35f;        // safe-cutoff voltage

// Bluetooth instance (ESP32 built-in, no extra hardware)
BluetoothSerial SerialBT;

// ============================================================================
// BATTERY LOOK-UP TABLE — 11 points, BATT_V_MIN → BATT_V_MAX
//
// Maps linear ADC fraction to a nonlinear Li-ion discharge curve.
// Index 0 = BATT_V_MIN, Index 10 = BATT_V_MAX.
// Tune midpoint values to match your cell's knee.
// ============================================================================

static constexpr int BATT_LUT[] = {
    0,    // 0  → empty
    0,    // 1
    5,    // 2
    10,   // 3
    15,   // 4  → ~3.48 V
    20,   // 5  → ~3.60 V
    30,   // 6  → ~3.72 V
    55,   // 7  → ~3.84 V
    75,   // 8  → ~3.96 V
    90,   // 9  → ~4.08 V
    100   // 10 → full
};

// ============================================================================
// ENUMERATIONS — strong types (enum class) prevent accidental conversions
// ============================================================================

enum class Mode {
    AUTO,     // track light automatically
    MANUAL,   // direct button / BT control
    SLEEP,    // light sleep, wakes on button or LDR change
    WRAP      // internal; executeWrap() unwinds stuck condition
};

enum class Dir {
    NONE = 0,
    LEFT,
    RIGHT
};

// ============================================================================
// GLOBAL STATE
// ============================================================================

Mode         currentMode     = Mode::AUTO;

// LDR sensors
int          leftLDR         = 0;
int          rightLDR        = 0;

// Battery
int          batteryPct      = 0;
int          batteryRaw      = 0;
float        batteryVolt     = 0.0f;

// Auto-mode stuck counter (5 consecutive → WRAP)
int          stuckCount      = 0;
Dir          lastDir         = Dir::NONE;

// Limiters (updated each loop by readLimiters())
bool         leftLimit       = false;
bool         rightLimit      = false;

// Timers (unsigned long for millis() arithmetic)
unsigned long idleTimer       = 0;   // last LDR diff event
unsigned long sleepCheckTimer = 0;   // last periodic LDR poll during sleep
unsigned long btTelemTimer    = 0;   // last BT telemetry send

// Sleep tone gate (play only once per sleep entry)
bool         sleepToneDone   = false;

// Button debounce state (edge-detection, 3 buttons)
unsigned long btnLastTime[3]  = {0, 0, 0};
bool          btnLastState[3] = {false, false, false};
bool          btnReady[3]     = {true, true, true};

// ============================================================================
// FORWARD DECLARATIONS — required by arduino-cli for .ino single-file builds
// ============================================================================

// Audio feedback
void playBootTone();
void playMenuTone();
void playButtonTone();
void playSleepTone();
void playWakeTone();

// Mode handlers
void handleSleep();
void autoMode();
void manualMode();
void wrapMode();
void executeWrap(Dir blockedDir);

// Hardware I/O
void pulseServo(Dir dir);
void readLDRs();
void readLimiters();
bool debouncedRead(int pin);
void readBattery();

// Helpers
bool limiterTouched();
Dir  towardLimiter();
bool btnPressed(int pin);
bool btnHeld(int pin);
bool checkSelectButton();
bool anyButtonPressed();
int  btnIndex(int pin);

// Display
void updateDisplay();
void displayMenu(const char* selected);
void displaySleep();

// Bluetooth
void checkBluetooth();
void sendTelemetry();

// ============================================================================
// SETUP — runs once at power-on / reset
// ============================================================================

void setup() {
    // ── Serial & Bluetooth ──
    Serial.begin(115200);
    analogSetAttenuation(ADC_11db);              // 0–3.6 V range for battery ADC
    SerialBT.begin("SolarTracker");

    // ── GPIO ──
    pinMode(PIN_LIM_L, INPUT_PULLUP);            // limiters pull LOW on contact
    pinMode(PIN_LIM_R, INPUT_PULLUP);
    pinMode(PIN_BTN_L, INPUT_PULLUP);            // buttons pull LOW on press
    pinMode(PIN_BTN_R, INPUT_PULLUP);
    pinMode(PIN_BTN_S, INPUT_PULLUP);
    pinMode(PIN_BUZZ,  OUTPUT);

    // ── Servo ──
    servo.attach(PIN_SERVO);
    servo.write(90);                             // stop (1500 µs)

    // ── OLED ──
    Wire.begin(OLED_SDA, OLED_SCL);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.display();

    // ── Startup ──
    playBootTone();
    idleTimer = millis();
}

// ============================================================================
// MAIN LOOP — called repeatedly by Arduino runtime
//
// Priority order:
//   1. Sleep gate          → skip all processing, conserve power
//   2. Sensors             → read LDRs, battery
//   3. Bluetooth command   → process remote input
//   4. Display             → update OLED
//   5. Serial debug        → print status line
//   6. Select button       → open menu (blocks until exit)
//   7. Mode dispatch       → execute current mode logic
// ============================================================================

void loop() {

    // Gate: if sleeping, do nothing else
    if (currentMode == Mode::SLEEP) {
        handleSleep();
        return;
    }

    // Reset sleep tone gate (so it plays again on next sleep entry)
    sleepToneDone = false;

    // ── Sensors ──
    readLDRs();
    readBattery();

    // ── Bluetooth ──
    checkBluetooth();

    // ── OLED ──
    updateDisplay();

    // ── Serial debug (one compact line per loop) ──
    Serial.print("L:");   Serial.print(leftLDR);
    Serial.print(" R:");  Serial.print(rightLDR);
    Serial.print(" d:");  Serial.print(leftLDR - rightLDR);
    Serial.print(" LL:"); Serial.print(leftLimit  ? "T" : "-");
    Serial.print(" RL:"); Serial.print(rightLimit ? "T" : "-");
    Serial.print(" B:");  Serial.print(batteryPct);
    Serial.print("%(");   Serial.print(batteryRaw);
    Serial.print(",");    Serial.print(batteryVolt, 2);
    Serial.print("V) M:");
    switch (currentMode) {
        case Mode::AUTO:   Serial.print("AUTO"); break;
        case Mode::MANUAL: Serial.print("MAN");  break;
        case Mode::SLEEP:  Serial.print("SLP");  break;
        case Mode::WRAP:   Serial.print("WRP");  break;
    }
    Serial.println();

    // ── Select button → menu (edge-triggered, blocks until menu exits) ──
    if (checkSelectButton()) {
        playMenuTone();
        menu();
        return;  // re-enter loop fresh after menu
    }

    // ── Mode dispatch ──
    switch (currentMode) {
        case Mode::AUTO:   autoMode();   break;
        case Mode::MANUAL: manualMode(); break;
        case Mode::WRAP:   wrapMode();   break;
        default: break;
    }
}

// ============================================================================
// MENU — blocking navigation, edge-triggered buttons
//
// Left  = previous mode (wraps)
// Right = next mode (wraps)
// Select = confirm and exit
// ============================================================================

void menu() {
    const char* names[] = { "AUTO", "MANUAL", "SLEEP" };
    int idx = static_cast<int>(currentMode);   // start at current mode

    while (true) {
        displayMenu(names[idx]);

        if (btnPressed(PIN_BTN_L)) {
            playButtonTone();
            idx = (idx + 2) % 3;               // wrap left
            delay(MENU_DEBOUNCE_MS);
        }
        if (btnPressed(PIN_BTN_R)) {
            playButtonTone();
            idx = (idx + 1) % 3;               // wrap right
            delay(MENU_DEBOUNCE_MS);
        }
        if (btnPressed(PIN_BTN_S)) {
            playButtonTone();
            currentMode = static_cast<Mode>(idx);
            delay(MENU_DEBOUNCE_MS);
            return;
        }
    }
}

// ============================================================================
// MANUAL MODE — direct servo control
//
// Holds direction button → pulse servo each loop.
// Limiter prevents movement INTO the contacted switch.
// ============================================================================

void manualMode() {

    // Determine intended direction from held button (level, not edge)
    Dir dir = Dir::NONE;
    if      (btnHeld(PIN_BTN_L)) dir = Dir::LEFT;
    else if (btnHeld(PIN_BTN_R)) dir = Dir::RIGHT;

    if (dir == Dir::NONE) return;    // no button held

    readLimiters();

    // Block: don't move into the limiter that's already contacting
    if (limiterTouched() && dir == towardLimiter()) {
        Serial.println("BLOCKED: moving into limiter");
        return;
    }

    Serial.print("PULSE MANUAL: ");
    Serial.println(dir == Dir::LEFT ? "LEFT" : "RIGHT");
    pulseServo(dir);
    delay(PULSE_WAIT_MS);
}

// ============================================================================
// AUTO MODE — closed-loop solar tracking
//
//   LDR diff < threshold  →  centered, do nothing
//   Limiter not touched    →  pulse toward light, reset stuck counter
//   Limiter touched, away  →  pulse (safe), reset stuck counter
//   Limiter touched, into  →  block & increment stuck counter
//   Stuck ≥ 5              →  enter WRAP → executeWrap()
// ============================================================================

void autoMode() {
    int diff = leftLDR - rightLDR;

    // Centered — check idle timer, then bail
    if (abs(diff) < LDR_THRESH) {
        if (millis() - idleTimer > IDLE_MS) {
            currentMode = Mode::SLEEP;         // auto sleep after idle
        }
        return;
    }

    // There IS a meaningful difference — reset idle timer
    idleTimer = millis();
    readLimiters();

    Dir want = (diff > 0) ? Dir::LEFT : Dir::RIGHT;

    // ── No limiter touched → safe, pulse ──
    if (!limiterTouched()) {
        stuckCount = 0;
        Serial.print("PULSE AUTO: ");
        Serial.println(want == Dir::LEFT ? "LEFT" : "RIGHT");
        pulseServo(want);
        return;
    }

    // ── Limiter touched, but moving away → safe, pulse ──
    if (want != towardLimiter()) {
        stuckCount = 0;
        Serial.print("PULSE AUTO (away): ");
        Serial.println(want == Dir::LEFT ? "LEFT" : "RIGHT");
        pulseServo(want);
        return;
    }

    // ── Limiter touched AND moving into it → block ──
    if (stuckCount < 5) {
        stuckCount++;
        Serial.print("BLOCKED stuckCount=");
        Serial.println(stuckCount);
        return;
    }

    // ── Stuck 5 consecutive times → unwrap ──
    Serial.println("WRAP!");
    currentMode = Mode::WRAP;
    executeWrap(want);        // blocking; handles full unwrap sequence
    currentMode = Mode::AUTO; // restore normal operation
    stuckCount = 0;
}

// ============================================================================
// WRAP MODE — stuck-limiter unwinding
//
// Called internally from autoMode.  Not a user-facing mode.
//
// Sequence:
//   1. Continuous full-speed rotation toward OPPOSITE limiter
//   2. Stop when that limiter is contacted
//   3. Back off with N escape pulses away from arrived limiter
//
// Params:
//   blockedDir — the direction that was blocked (limiter side)
// ============================================================================

void wrapMode() {
    // Stub — actual logic lives in executeWrap() called from autoMode.
    // This exists so the switch-case in loop() doesn't crash.
}

void executeWrap(Dir blockedDir) {
    Dir escapeDir = (blockedDir == Dir::LEFT) ? Dir::RIGHT : Dir::LEFT;

    Serial.print("WRAP: rotating ");
    Serial.println(escapeDir == Dir::LEFT ? "LEFT" : "RIGHT");

    // ── Phase 1: continuous rotation to other limiter ──
    int speedVal = (escapeDir == Dir::LEFT) ? 0 : 180;
    servo.write(speedVal);                       // full speed

    unsigned long start = millis();
    const unsigned long timeout = 10000;          // 10 s safety ceiling

    while (millis() - start < timeout) {
        readLimiters();
        if (escapeDir == Dir::LEFT  && leftLimit) {
            Serial.println("WRAP: hit left limiter");
            break;
        }
        if (escapeDir == Dir::RIGHT && rightLimit) {
            Serial.println("WRAP: hit right limiter");
            break;
        }
        delay(10);                               // poll limiter every 10 ms
    }

    servo.write(90);                             // stop

    // ── Phase 2: escape pulses away from arrived limiter ──
    Serial.print("WRAP: escaping ");
    Serial.print(WRAP_ESCAPE_PULSES);
    Serial.println(" pulses");

    for (int i = 0; i < WRAP_ESCAPE_PULSES; i++) {
        int escapeVal = (blockedDir == Dir::LEFT) ? 0 : 180;
        servo.write(escapeVal);
        delay(WRAP_ESCAPE_MS);
        servo.write(90);
        delay(PULSE_WAIT_MS);
    }

    Serial.println("WRAP: done");
}

// ============================================================================
// SLEEP MODE — power conservation
//
// On entry:   play descending tone, display "SLEEP"
// Every loop: check buttons (instant wake), check LDRs (periodic)
// If no wake condition: enter ESP32 light sleep for 500 ms
//
// Wake sources:
//   - Any button pressed     → go to AUTO
//   - LDR diff ≥ threshold   → go to AUTO (checked every SLEEP_CHECK_MS)
// ============================================================================

void handleSleep() {

    // One-shot: play sleep tone on first entry
    if (!sleepToneDone) {
        playSleepTone();
        sleepToneDone = true;
    }

    displaySleep();

    unsigned long now = millis();

    // ── Periodic LDR poll during sleep ──
    if (now - sleepCheckTimer >= SLEEP_CHECK_MS) {
        sleepCheckTimer = now;
        readLDRs();
        if (abs(leftLDR - rightLDR) >= LDR_THRESH) {
            Serial.println("WAKE: LDR diff");
            playWakeTone();
            sleepToneDone = false;
            currentMode = Mode::AUTO;
            return;
        }
    }

    // ── Button wake (raw level check — no debounce needed here) ──
    if (digitalRead(PIN_BTN_L) == LOW ||
        digitalRead(PIN_BTN_R) == LOW ||
        digitalRead(PIN_BTN_S) == LOW) {
        Serial.println("WAKE: button");
        playWakeTone();
        sleepToneDone = false;
        currentMode = Mode::AUTO;
        return;
    }

    // ── Enter light sleep (wakes on timer after 500 ms) ──
    esp_sleep_enable_timer_wakeup(500000ULL);  // 500 ms, wakes to poll again
    esp_light_sleep_start();
}

// ============================================================================
// SERVO PULSE — non-blocking single step
//
// Writes speed value, waits PULSE_MS, then stops at 90° (1500 µs).
// For 360° continuous servos:
//   0°   = 1000 µs = full speed clockwise
//   90°  = 1500 µs = stop
//   180° = 2000 µs = full speed counter-clockwise
// ============================================================================

void pulseServo(Dir dir) {
    int val = (dir == Dir::LEFT) ? 0 : 180;
    servo.write(val);
    delay(PULSE_MS);
    servo.write(90);
    lastDir = dir;
}

// ============================================================================
// SENSORS
// ============================================================================

/**
 * Read LDRs with hardware oversampling to reduce noise.
 * LDR_SAMPLES controls the averaging window (1024 ≈ 10 ms per channel).
 */
void readLDRs() {
    long sumL = 0, sumR = 0;
    for (int i = 0; i < LDR_SAMPLES; i++) {
        sumL += analogRead(PIN_LDR_L);
        sumR += analogRead(PIN_LDR_R);
    }
    leftLDR  = static_cast<int>(sumL / LDR_SAMPLES);
    rightLDR = static_cast<int>(sumR / LDR_SAMPLES);
}

/**
 * Read copper-bar limit switches with software debounce.
 */
void readLimiters() {
    leftLimit  = debouncedRead(PIN_LIM_L);
    rightLimit = debouncedRead(PIN_LIM_R);
}

/**
 * Debounced digital read — takes 5 samples over 5 ms,
 * requires ≥ 4 LOW to register as pressed.
 * Eliminates false triggers from copper-bar bounce.
 */
bool debouncedRead(int pin) {
    int count = 0;
    for (int i = 0; i < 5; i++) {
        if (digitalRead(pin) == LOW) count++;
        delay(1);
    }
    return count >= 4;
}

/**
 * Read battery voltage through voltage divider.
 *
 * Formula:
 *   batteryRaw = ADC raw (0–4095)
 *   batteryVolt = raw × (3.3 V / 4095) × BATT_DIVIDER
 *   fraction    = (batteryVolt - BATT_V_MIN) / (BATT_V_MAX - BATT_V_MIN)
 *   index       = fraction × 10  (clamped 0..10)
 *   batteryPct  = BATT_LUT[index]
 */
void readBattery() {
    batteryRaw = analogRead(PIN_BATT);
    batteryVolt = batteryRaw * (3.3f / 4095.0f) * BATT_DIVIDER;
    float frac = (batteryVolt - BATT_V_MIN) / (BATT_V_MAX - BATT_V_MIN);
    int idx = constrain((int)(frac * 10.0f), 0, 10);
    batteryPct = BATT_LUT[idx];
}

// ============================================================================
// HELPERS
// ============================================================================

/** True if either limiter is contacting. */
bool limiterTouched() {
    return leftLimit || rightLimit;
}

/**
 * Returns which limiter is currently contacted.
 * If both are touched (unlikely), returns the one checked first (left).
 */
Dir towardLimiter() {
    if (leftLimit)  return Dir::LEFT;
    if (rightLimit) return Dir::RIGHT;
    return Dir::NONE;
}

/** Map pin number → array index for button debounce state. */
int btnIndex(int pin) {
    if (pin == PIN_BTN_L) return 0;
    if (pin == PIN_BTN_R) return 1;
    return 2;  // PIN_BTN_S
}

/**
 * Edge-detecting button press.
 *
 * State machine:
 *   1. Wait for stable reading (≥ BTN_DEBOUNCE_MS)
 *   2. On stable LOW with btnReady flag → return true once
 *   3. Clear btnReady (prevents re-trigger while held)
 *   4. On release (HIGH) → restore btnReady
 *
 * Use for menu navigation and select button.
 */
bool btnPressed(int pin) {
    int idx = btnIndex(pin);
    bool reading = (digitalRead(pin) == LOW);
    unsigned long now = millis();

    // State change — reset timer, do not fire
    if (reading != btnLastState[idx]) {
        btnLastTime[idx]  = now;
        btnLastState[idx] = reading;
        return false;
    }

    // Stable window passed
    if ((now - btnLastTime[idx]) > BTN_DEBOUNCE_MS) {
        if (reading && btnReady[idx]) {
            btnReady[idx] = false;   // fire once, then block
            return true;
        }
        if (!reading) {
            btnReady[idx] = true;    // button released, re-arm
        }
    }
    return false;
}

/**
 * Raw level check for directional buttons.
 * Used in MANUAL mode where holding should continuously pulse.
 */
bool btnHeld(int pin) {
    return digitalRead(pin) == LOW;
}

/** Edge-triggered select button check. */
bool checkSelectButton() {
    return btnPressed(PIN_BTN_S);
}

/** Edge-triggered any-button check (for wake events). */
bool anyButtonPressed() {
    return btnPressed(PIN_BTN_L) || btnPressed(PIN_BTN_R) || btnPressed(PIN_BTN_S);
}

// ============================================================================
// BLUETOOTH — ESP32 Classic SPP
//
// Pair from phone as "SolarTracker" (no PIN).
// Telemetry auto-sent every BT_TELEM_MS.
// Commands are case-insensitive, newline-terminated.
// ============================================================================

void checkBluetooth() {

    // Periodic telemetry
    if (millis() - btTelemTimer >= BT_TELEM_MS) {
        btTelemTimer = millis();
        sendTelemetry();
    }

    // Process incoming commands
    while (SerialBT.available()) {
        String cmd = SerialBT.readStringUntil('\n');
        cmd.trim();
        cmd.toUpperCase();

        if (cmd == "A" || cmd == "AUTO") {
            currentMode = Mode::AUTO;
            SerialBT.println("OK AUTO");
        }
        else if (cmd == "M" || cmd == "MANUAL") {
            currentMode = Mode::MANUAL;
            SerialBT.println("OK MANUAL");
        }
        else if (cmd == "S" || cmd == "SLEEP") {
            currentMode = Mode::SLEEP;
            SerialBT.println("OK SLEEP");
        }
        else if (cmd == "L" || cmd == "LEFT") {
            if (currentMode == Mode::MANUAL) {
                pulseServo(Dir::LEFT);
                SerialBT.println("OK LEFT");
            } else {
                SerialBT.println("ERR: not in MANUAL");
            }
        }
        else if (cmd == "R" || cmd == "RIGHT") {
            if (currentMode == Mode::MANUAL) {
                pulseServo(Dir::RIGHT);
                SerialBT.println("OK RIGHT");
            } else {
                SerialBT.println("ERR: not in MANUAL");
            }
        }
        else if (cmd == "?" || cmd == "STATUS") {
            sendTelemetry();
        }
        else if (cmd.length() > 0) {
            SerialBT.println("ERR: unknown cmd");
        }
    }
}

/**
 * Send compact telemetry string over Bluetooth.
 * Format: MODE:AUTO B:85 L:2045 R:1890 D:LEFT
 */
void sendTelemetry() {
    int diff = leftLDR - rightLDR;
    const char* dir = (abs(diff) < LDR_THRESH) ? "CENTER"
                    : (diff > 0)            ? "LEFT"
                    :                         "RIGHT";

    const char* mode = (currentMode == Mode::AUTO)   ? "AUTO"
                     : (currentMode == Mode::MANUAL) ? "MANUAL"
                     : (currentMode == Mode::SLEEP)  ? "SLEEP"
                     :                                 "WRAP";

    SerialBT.print("MODE:"); SerialBT.print(mode);
    SerialBT.print(" B:");   SerialBT.print(batteryPct);
    SerialBT.print(" L:");   SerialBT.print(leftLDR);
    SerialBT.print(" R:");   SerialBT.print(rightLDR);
    SerialBT.print(" D:");   SerialBT.println(dir);
}

// ============================================================================
// DISPLAY — SSD1306 128×64 OLED
// ============================================================================

void updateDisplay() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    const char* modeName = (currentMode == Mode::AUTO)   ? "AUTO"
                         : (currentMode == Mode::MANUAL) ? "MANUAL"
                         : (currentMode == Mode::SLEEP)  ? "SLEEP"
                         :                                 "WRAP";

    display.print("MODE:"); display.println(modeName);
    display.println();                                   // blank line
    display.print("B:"); display.print(batteryPct); display.println("%");
    display.println();
    display.print("L:"); display.print(leftLDR);
    display.print(" R:"); display.print(rightLDR);
    display.println();
    display.print("Dir: ");
    int diff = leftLDR - rightLDR;
    if (abs(diff) < LDR_THRESH) {
        display.println("CENTER");
    } else {
        display.println((diff > 0) ? "LEFT" : "RIGHT");
    }
    display.println();
    display.print("[SEL] Menu");

    display.display();
}

void displayMenu(const char* selected) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("=== MENU ===");
    display.println();
    display.print("> ");
    display.println(selected);
    display.println();
    display.println("L/R: change");
    display.println("SEL: confirm");
    display.display();
}

void displaySleep() {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 24);
    display.println("SLEEP");
    display.display();
}

// ============================================================================
// AUDIO — Piezo buzzer feedback tones
//
// All tones use the non-blocking tone() function.
// Short delays are acceptable here because these fire on events,
// not inside the main control loop.
// ============================================================================

/** Ascending three-note boot jingle (C5 → E5 → G5). */
void playBootTone() {
    tone(PIN_BUZZ, 523, 200); delay(250);
    tone(PIN_BUZZ, 659, 200); delay(250);
    tone(PIN_BUZZ, 784, 200); delay(250);
    noTone(PIN_BUZZ);
}

/** Ascending two-note menu-open chime. */
void playMenuTone() {
    tone(PIN_BUZZ, 660, 80); delay(100);
    tone(PIN_BUZZ, 880, 80); delay(100);
    noTone(PIN_BUZZ);
}

/** Single 30 ms click for button feedback. */
void playButtonTone() {
    tone(PIN_BUZZ, 1000, 30);
}

/** Descending two-note sleep lullaby. */
void playSleepTone() {
    tone(PIN_BUZZ, 660, 100); delay(150);
    tone(PIN_BUZZ, 440, 200); delay(250);
    noTone(PIN_BUZZ);
}

/** Ascending two-note wake-up chime. */
void playWakeTone() {
    tone(PIN_BUZZ, 440, 100); delay(150);
    tone(PIN_BUZZ, 660, 150); delay(200);
    noTone(PIN_BUZZ);
}
