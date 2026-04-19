/*
 * Sun Tracker — ESP32
 * ====================
 * Autonomous sun tracking system with dual LDR sensing and OLED UI.
 */

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <Wire.h>

// --- Pins ---
static constexpr uint8_t PIN_SERVO = 12;
static constexpr uint8_t PIN_LDR_LEFT = 34;
static constexpr uint8_t PIN_LDR_RIGHT = 35;
static constexpr uint8_t PIN_BUZZER = 14;
static constexpr uint8_t PIN_OLED_SDA = 21;
static constexpr uint8_t PIN_OLED_SCL = 22;
static constexpr uint8_t PIN_BTN_LEFT = 4;
static constexpr uint8_t PIN_BTN_SELECT = 18;
static constexpr uint8_t PIN_BTN_RIGHT = 19;
static constexpr uint8_t PIN_LIMIT_LEFT = 13;
static constexpr uint8_t PIN_LIMIT_RIGHT = 15;
static constexpr uint8_t PIN_BATTERY = 36;

// --- Config ---
static constexpr uint8_t SCREEN_WIDTH = 128;
static constexpr uint8_t SCREEN_HEIGHT = 64;
static constexpr int8_t OLED_RESET = -1;
static constexpr uint8_t SCREEN_ADDRESS = 0x3C;

static constexpr uint8_t SERVO_STOP = 90;
static constexpr uint8_t SERVO_RIGHT = 0;
static constexpr uint8_t SERVO_LEFT = 180;
static constexpr int8_t SERVO_TRIM = 0;
static constexpr uint8_t SERVO_STOP_REAL = SERVO_STOP + SERVO_TRIM;

// ==================== LDR SETTINGS ====================
// LDR_SAMPLES: How many ADC reads to average (smooths jitter)
// LDR_DEADZONE: Stop if light diff < this (prevents micro-jitter)
// LDR_OFFSET: Calibrate sensor imbalance (if one reads higher)
// LDR_HYSTERESIS: Need this much diff to reverse direction (stops flip-flop)
static constexpr uint16_t LDR_SAMPLES = 128;    // Samples per reading (higher = smoother)
static constexpr uint16_t LDR_DEADZONE = 200;    // Stop if |diff| < 200
static constexpr int16_t LDR_OFFSET = 0;       // Calibration offset
static constexpr int16_t LDR_HYSTERESIS = 80;   // Reverse requires >80 diff
static constexpr uint16_t NIGHT_THRESHOLD = 500; // Night detection (both LDRs < this = dark)

// ==================== TIMING ====================
// INTERVAL_LDR: How often to read sensors (ms)
// INTERVAL_DISPLAY: How often to update OLED (ms)
// INTERVAL_BUTTONS: How often to check buttons (ms)
static constexpr uint32_t INTERVAL_LDR = 200;
static constexpr uint32_t INTERVAL_DISPLAY = 100;
static constexpr uint32_t INTERVAL_BUTTONS = 20;
static constexpr uint32_t DEBOUNCE_MS = 50;

// ==================== POWER ====================
// IDLE_TIMEOUT: Minutes idle before deep sleep
// SLEEP_CHECK_INTERVAL: Minutes between wake checks
static constexpr uint32_t IDLE_TIMEOUT = 1;        // Minutes of no movement → sleep
static constexpr uint32_t SLEEP_CHECK_INTERVAL = 30; // Seconds between wake checks

// ==================== SERVO ====================
// SERVO_PULSE_ON: Time servo runs per pulse (ms)
// SERVO_PULSE_OFF: Time servo stops per pulse (ms)
// PULSE_BATCH: Move this many pulses before checking
static constexpr uint32_t SERVO_PULSE_ON = 15;    // Servo ON time (ms)
static constexpr uint32_t SERVO_PULSE_OFF = 250;  // Servo OFF time (ms)
static constexpr uint8_t PULSE_BATCH = 1;       // Pulses before check

// --- Objects ---
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Servo sunServo;

// --- State ---
enum Mode {
  MODE_AUTO,
  MODE_MANUAL,
  MODE_STANDBY
};
enum State {
  STATE_ACTIVE,
  STATE_NIGHT,
  STATE_SLEEP
};
enum MenuCtx {
  MENU_NONE,
  MENU_MAIN,
  MENU_SETTINGS
};

Mode mode = MODE_AUTO;
State state = STATE_ACTIVE;
MenuCtx menuCtx = MENU_NONE;
uint8_t menuIdx = 0;
uint8_t menuCnt = 0;

// Forward decl needed for menu
void actionAuto();
void actionManual();
void actionSleep();
void actionBack();

struct MenuItem {
  const char *label;
  void (*action)();
};

const MenuItem *currentMenu = nullptr;

static const MenuItem MAIN_MENU[] = { { "Auto Track", actionAuto },
                                      { "Manual", actionManual },
                                      { "Sleep", actionSleep },
                                      { "Back", actionBack } };
static constexpr uint8_t MAIN_MENU_CNT = 4;
// --- Battary Data ---
uint8_t batteryPct = 0;

// --- LDR Data ---
uint16_t ldrLeft = 0;
uint16_t ldrRight = 0;
uint16_t ldrBufL[5] = { 0 };
uint16_t ldrBufR[5] = { 0 };
uint8_t ldrIdx = 0;

// --- Timing ---
uint32_t lastLDR = 0;
uint32_t lastDisplay = 0;
uint32_t lastBtn = 0;
uint32_t lastPulse = 0;
uint32_t lastMoveTime = 0;
bool pulseOn = false;

// --- Buttons ---
struct Btn {
  bool pressed = false;
  bool lastRaw = false;
  uint32_t lastChange = 0;
};
Btn btns[3];

// --- Animation ---
float sunPulse = 0.0f;
struct Anim {
  uint8_t frame = 0;
  uint8_t maxFrame = 0;
  uint32_t interval = 0;
  uint32_t lastFrame = 0;
  bool playing = false;
};
Anim bootAnim = { 0, 20, 80, 0, false };
Anim menuAnim = { 0, 6, 50, 0, false };

// --- Tone ---
struct Tone {
  const char *pattern;
  uint8_t ms;
};
static constexpr Tone TONE_START = { "101011", 120 };
static constexpr Tone TONE_SCROLL = { "1", 60 };
static constexpr Tone TONE_SELECT = { "101", 100 };
static constexpr Tone TONE_SLEEP = { "10101000", 150 };

struct TonePlayer {
  const char *pattern = nullptr;
  uint8_t ms = 100;
  uint8_t idx = 0;
  uint32_t lastBeat = 0;
  bool playing = false;
} tonePlayer;

// --- Forward ---
void openMenu(const MenuItem *m, uint8_t c);
void closeMenu();
void playTone(const Tone &t);
void updateTone();
uint16_t readLDR(uint8_t pin);
bool btnPressed(uint8_t pin, uint8_t idx);
void handleBtns();
void trackSun(uint32_t now);
void goToSleep();
void drawBoot();
void drawDisplay();
void actionBack();
void actionAuto();
void actionManual();
void actionSleep();

// --- Menu Actions ---
void actionAuto() {
  mode = MODE_AUTO;
  state = STATE_ACTIVE;
  closeMenu();
}
void actionManual() {
  mode = MODE_MANUAL;
  state = STATE_ACTIVE;
  closeMenu();
}
void actionSleep() {
  closeMenu();
  goToSleep();
}
void actionBack() {
  openMenu(MAIN_MENU, MAIN_MENU_CNT);
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Sun Tracker ===");

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
  pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_BTN_SELECT, INPUT_PULLUP);
  pinMode(PIN_LIMIT_LEFT, INPUT_PULLUP);
  pinMode(PIN_LIMIT_RIGHT, INPUT_PULLUP);
  pinMode(PIN_BATTERY, INPUT);

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED fail!");
    while (1)
      ;
  }

  Serial.println("Boot anim...");
  bootAnim.playing = true;
  bootAnim.frame = 1;
  while (bootAnim.playing) {
    if (millis() - bootAnim.lastFrame >= bootAnim.interval) {
      bootAnim.lastFrame = millis();
      bootAnim.frame++;
      if (bootAnim.frame > bootAnim.maxFrame)
        bootAnim.playing = false;
    }
    delay(10);
  }

  sunServo.attach(PIN_SERVO);
  sunServo.write(SERVO_STOP_REAL);

  ldrLeft = readLDR(PIN_LDR_LEFT);
  ldrRight = readLDR(PIN_LDR_RIGHT);
  Serial.printf("LDR: %u | %u\n", ldrLeft, ldrRight);

  // Night detection - enable by changing to true if needed
  bool enableNightDetect = false;
  if (enableNightDetect && ldrLeft < NIGHT_THRESHOLD && ldrRight < NIGHT_THRESHOLD) {
    Serial.println("Night - sleep");
    state = STATE_NIGHT;
    goToSleep();
  }

  playTone(TONE_START);
  Serial.println("Ready");
}

// --- Loop ---
void loop() {
  uint32_t now = millis();

  updateTone();

  if (now - lastPulse >= (pulseOn ? SERVO_PULSE_ON : SERVO_PULSE_OFF)) {
    lastPulse = now;
    pulseOn = !pulseOn;
  }

  if (now - lastLDR >= INTERVAL_LDR) {
    lastLDR = now;
    uint16_t rawL = readLDR(PIN_LDR_LEFT);
    uint16_t rawR = readLDR(PIN_LDR_RIGHT);
    ldrBufL[ldrIdx] = rawL;
    ldrBufR[ldrIdx] = rawR;
    ldrIdx = (ldrIdx + 1) % 5;
    uint32_t sumL = 0, sumR = 0;
    for (uint8_t i = 0; i < 5; i++) {
      sumL += ldrBufL[i];
      sumR += ldrBufR[i];
    }
    ldrLeft = sumL / 5;
    ldrRight = sumR / 5;

    // Read battery (every ~1 second)
    static uint32_t lastBat = 0;
    if (now - lastBat >= 1000) {
      lastBat = now;
      uint16_t batRaw = analogRead(PIN_BATTERY);
      float batVolts = (batRaw / 4095.0f) * 3.3f * 2.0f;
      Serial.printf("BatADC: %u = %.2fV\n", batRaw, batVolts);
      // 18650: 3.0V = 0%, 4.2V = 100%
      int pct = (int)((batVolts - 3.0f) * 100.0f / 1.2f);
      batteryPct = constrain(pct, 0, 100);
      Serial.printf("Bat: %u%%\n", batteryPct);
    }

    int16_t diff = (ldrLeft + LDR_OFFSET) - ldrRight;
    Serial.printf("L:%u R:%u Diff:%d ", ldrLeft, ldrRight, diff);

    if (abs(diff) < LDR_DEADZONE)
      Serial.println("HOLD");
    else if (diff > 0)
      Serial.println("LEFT");
    else if (diff < 0)
      Serial.println("RIGHT");
  }

  if (now - lastBtn >= INTERVAL_BUTTONS) {
    lastBtn = now;
    handleBtns();
  }

  trackSun(now);

  // Check idle timeout - deep sleep if no movement
  if (mode == MODE_AUTO && (now - lastMoveTime) >= (IDLE_TIMEOUT * 60UL * 1000UL)) {
    Serial.println("Idle - deep sleep");
    goToSleep();
    return;
  }

  if (now - lastDisplay >= INTERVAL_DISPLAY) {
    lastDisplay = now;
    drawDisplay();
  }
}

// --- Functions ---
static bool lastDir = false;
static uint8_t pulseCount = 0;

void trackSun(uint32_t now) {
  if (mode != MODE_AUTO)
    return;

  // Check limit switches - stop at limit, allow opposite
  bool hitLeft = digitalRead(PIN_LIMIT_LEFT) == LOW;
  bool hitRight = digitalRead(PIN_LIMIT_RIGHT) == LOW;

  // At limit - reverse immediately for 360 effect
  bool forceReverse = false;
  if (hitLeft) {
    lastDir = false;
    Serial.println("LIMIT LEFT → REVERSE");
    forceReverse = true;
  } else if (hitRight) {
    lastDir = true;
    Serial.println("LIMIT RIGHT → REVERSE");
    forceReverse = true;
  }

  int16_t diff = (ldrLeft + LDR_OFFSET) - ldrRight;
  bool newDir = diff > 0;

  // Skip normal checks if forced reverse
  if (!forceReverse) {
    if (abs(diff) < LDR_DEADZONE) {
      sunServo.write(SERVO_STOP_REAL);
      pulseCount = 0;
      return;
    }

    // Check hysteresis before reversing
    if (newDir != lastDir && abs(diff) < LDR_HYSTERESIS) {
      sunServo.write(SERVO_STOP_REAL);
      return;
    }
    lastDir = newDir;
  }

  // Move batch of pulses, then check again
  if (pulseOn) {
    pulseCount++;
    lastMoveTime = now;
    sunServo.write(lastDir ? SERVO_RIGHT : SERVO_LEFT);
  } else {
    sunServo.write(SERVO_STOP_REAL);
    if (pulseCount >= PULSE_BATCH) {
      pulseCount = 0;
      return;
    }
  }
}

bool btnPressed(uint8_t pin, uint8_t idx) {
  bool raw = digitalRead(pin) == LOW;
  uint32_t now = millis();
  if (raw != btns[idx].lastRaw)
    btns[idx].lastChange = now;
  btns[idx].lastRaw = raw;
  if (raw && now - btns[idx].lastChange >= DEBOUNCE_MS) {
    if (!btns[idx].pressed) {
      btns[idx].pressed = true;
      return true;
    }
  } else if (!raw) {
    btns[idx].pressed = false;
  }
  return false;
}

void handleBtns() {
  if (btnPressed(PIN_BTN_LEFT, 0)) {
    if (menuCtx != MENU_NONE && menuIdx > 0)
      menuIdx--;
    else if (mode == MODE_MANUAL || mode == MODE_STANDBY)
      sunServo.write(SERVO_LEFT);
    playTone(TONE_SCROLL);
  }
  if (btnPressed(PIN_BTN_RIGHT, 1)) {
    if (menuCtx != MENU_NONE && menuIdx < menuCnt - 1)
      menuIdx++;
    else if (mode == MODE_MANUAL || mode == MODE_STANDBY)
      sunServo.write(SERVO_RIGHT);
    playTone(TONE_SCROLL);
  }
  if (btnPressed(PIN_BTN_SELECT, 2)) {
    if (menuCtx != MENU_NONE && currentMenu && currentMenu[menuIdx].action) {
      currentMenu[menuIdx].action();
    } else {
      openMenu(MAIN_MENU, MAIN_MENU_CNT);
    }
    playTone(TONE_SELECT);
  }
}

void playTone(const Tone &t) {
  tonePlayer.pattern = t.pattern;
  tonePlayer.ms = t.ms;
  tonePlayer.idx = 0;
  tonePlayer.lastBeat = millis();
  tonePlayer.playing = true;
}

void updateTone() {
  if (!tonePlayer.playing || !tonePlayer.pattern)
    return;
  uint32_t now = millis();
  if (now - tonePlayer.lastBeat >= tonePlayer.ms) {
    tonePlayer.lastBeat = now;
    char b = tonePlayer.pattern[tonePlayer.idx];
    if (b == '\0') {
      digitalWrite(PIN_BUZZER, LOW);
      tonePlayer.playing = false;
    } else {
      digitalWrite(PIN_BUZZER, b == '1' ? HIGH : LOW);
      tonePlayer.idx++;
    }
  }
}

uint16_t readLDR(uint8_t pin) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < LDR_SAMPLES; i++) {
    sum += analogRead(pin);
  }
  return sum / LDR_SAMPLES;
}

void openMenu(const MenuItem *m, uint8_t c) {
  currentMenu = m;
  menuCnt = c;
  menuIdx = 0;
  menuCtx = MENU_MAIN;
  menuAnim.playing = true;
  menuAnim.frame = 0;
}

void closeMenu() {
  menuCtx = MENU_NONE;
  menuIdx = 0;
}

void drawDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (menuCtx == MENU_NONE) {
    const char *m = mode == MODE_AUTO     ? "AUTO"
                    : mode == MODE_MANUAL ? "MAN"
                                          : "STBY";
    display.setCursor(0, 0);
    display.print(m);

    display.setCursor(0, 16);
    display.printf("L:%u", ldrLeft);
    display.setCursor(0, 26);
    display.printf("R:%u", ldrRight);

    int16_t diff = (ldrLeft + LDR_OFFSET) - ldrRight;
    display.setCursor(0, 38);
    if (abs(diff) < LDR_DEADZONE)
      display.print("[=] CENTER");
    else if (diff > 0)
      display.print("[>] LEFT");
    else
      display.print("[<] RIGHT");

    display.setCursor(50, 0);
    display.printf("B:%u%%", batteryPct);

    display.setCursor(0, 56);
    display.print("[SEL] Menu");
  } else {
    display.setCursor(0, 0);
    display.print("== MENU ==");

    for (uint8_t i = 0; i < menuCnt && i < 4; i++) {
      display.setCursor(0, 16 + i * 10);
      if (i == menuIdx) {
        display.print(">");
      }
      display.print(currentMenu[i].label);
    }
  }
  display.display();
}

void goToSleep() {
  playTone(TONE_SLEEP);
  delay(800);
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 20);
  display.print("SLEEP");
  display.display();

  digitalWrite(PIN_BUZZER, LOW);
  sunServo.detach();

  esp_sleep_enable_timer_wakeup(30ULL * 1000000ULL);  // 30 seconds
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BTN_SELECT, LOW);

  delay(500);
  esp_deep_sleep_start();
}
