/*
 * Sun Tracker — ESP32
 * ====================
 * Autonomous sun tracking system with animated OLED UI.
 *
 * Hardware:
 *   - ESP-32S 30Pin (GPIO 12, 21, 22, 33, 34, 14, 4, 13, 15)
 *   - MG996R Servo 360° continuous rotation
 *   - LDR ×2 (voltage divider with 10kΩ)
 *   - OLED 0.96" SSD1306 I2C (128×64)
 *   - Active buzzer (rhythmic tone feedback)
 *   - Push buttons ×3 (LEFT, RIGHT, SELECT — INPUT_PULLUP, active LOW)
 *   - TP4056 + 18650 + solar panel (power chain)
 *
 * Engineering Decisions:
 *   - millis() timing (non-blocking) — no delay() in main loop
 *   - LDR oversampling (16 readings averaged) — cancels ADC noise
 *   - Button debounce via millis() — no false triggers
 *   - Deep sleep at night — 1000× power reduction
 *   - Serial at 115200 baud — ESP32 native speed
 *   - Fixed-width types (uint8_t/16_t/32_t) — no ambiguous "int"
 *   - Active buzzer with rhythmic patterns
 *   - 3-button menu navigation with animated OLED UI
 */

// ─── Libraries ───────────────────────────────────────────────
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <Wire.h>

// ─── Pin Definitions (GPIOs fit in uint8_t: 0-39) ───────────
static constexpr uint8_t PIN_SERVO = 12;
static constexpr uint8_t PIN_LDR_LEFT = 33;
static constexpr uint8_t PIN_LDR_RIGHT = 34;
static constexpr uint8_t PIN_BUZZER = 14;
static constexpr uint8_t PIN_OLED_SDA = 21;
static constexpr uint8_t PIN_OLED_SCL = 22;

// Buttons (INPUT_PULLUP, active LOW) — 3-button navigation
static constexpr uint8_t PIN_BTN_LEFT = 4;    // Scroll up / previous
static constexpr uint8_t PIN_BTN_RIGHT = 13;  // Scroll down / next
static constexpr uint8_t PIN_BTN_SELECT = 15; // Confirm / enter menu

// Wake-from-deep-sleep button (reuse select button)
static constexpr uint8_t PIN_BTN_WAKE = PIN_BTN_SELECT;

// ─── OLED ──────────────────────────────────────────────────
static constexpr uint8_t SCREEN_WIDTH = 128;
static constexpr uint8_t SCREEN_HEIGHT = 64;
static constexpr int8_t OLED_RESET = -1;
static constexpr uint8_t SCREEN_ADDRESS = 0x3C;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ─── Servo ───────────────────────────────────────────────────
Servo sunServo;

// 360° continuous rotation control values (fit in uint8_t: 0-180)
static constexpr uint8_t SERVO_STOP = 90;
static constexpr uint8_t SERVO_CW_SLOW = 60;   // Rotate clockwise (faster for testing)
static constexpr uint8_t SERVO_CCW_SLOW = 120; // Rotate counter-clockwise (faster for testing)

// Servo calibration: adjust this if the servo drifts at "stop"
// Positive = stops later (e.g., 92), Negative = stops earlier (e.g., 88)
static constexpr int8_t SERVO_TRIM = 0;
static constexpr uint8_t SERVO_STOP_REAL = 90 + SERVO_TRIM;

// ─── LDR Calibration ─────────────────────────────────────────
static constexpr uint8_t LDR_SAMPLES = 16;       // Oversample count
static constexpr uint16_t LDR_THRESHOLD = 120;   // Min diff to trigger movement
static constexpr uint16_t LDR_DEADZONE = 20;    // Ignore small differences
static constexpr uint16_t NIGHT_THRESHOLD = 500; // Below this on both LDRs = night

// LDR offset: balances manufacturing differences between the two LDRs
// Adjust this until Diff reads ~0 when both LDRs see the same light.
// Positive = Left reads higher, Negative = Right reads higher
static constexpr int16_t LDR_OFFSET = 150;

// ─── Timing (non-blocking, in milliseconds) ──────────────────
static constexpr uint32_t INTERVAL_LDR = 200;     // Read LDRs every 200ms
static constexpr uint32_t INTERVAL_DISPLAY = 100; // Update OLED every 100ms (for animation)
static constexpr uint32_t INTERVAL_BUTTONS = 20;  // Poll buttons every 20ms
static constexpr uint32_t DEBOUNCE_MS = 50;       // Button debounce window
static constexpr uint32_t SERVO_UPDATE_MS = 200;  // Servo update rate

// ─── Deep Sleep ──────────────────────────────────────────────
static constexpr uint32_t SLEEP_CHECK_INTERVAL = 30; // Minutes between wake checks

// ─── State Machine ───────────────────────────────────────────
enum SystemMode { MODE_AUTO, MODE_MANUAL, MODE_STANDBY };
enum SystemState { STATE_ACTIVE, STATE_NIGHT_SLEEP, STATE_FORCED_SLEEP };

SystemMode currentMode = MODE_AUTO;
SystemState currentState = STATE_ACTIVE;

// LDR values (ADC 0-4095 → uint16_t, not uint8_t which truncates to 255)
uint16_t ldrLeft = 0;
uint16_t ldrRight = 0;

// Rolling average buffers (smooths remaining noise after oversampling)
static constexpr uint8_t LDR_AVG_WINDOW = 5;
uint16_t ldrLeftBuf[LDR_AVG_WINDOW] = {0};
uint16_t ldrRightBuf[LDR_AVG_WINDOW] = {0};
uint8_t ldrBufIndex = 0;

// Timing variables (millis() returns uint32_t / unsigned long)
uint32_t lastLDRRead = 0;
uint32_t lastDisplayUpdate = 0;
uint32_t lastButtonCheck = 0;
uint32_t lastServoUpdate = 0;

// Button debounce tracker
struct ButtonState {
  bool pressed;
  bool lastRaw;
  uint32_t lastChange; // millis() timestamp
};
ButtonState btnStates[3] = {};

// ─── Menu System ─────────────────────────────────────────────
struct MenuItem {
  const char *label;
  void (*action)(void);
};

// Forward declarations for menu actions
void actionAutoTrack();
void actionManualControl();
void actionDeepSleep();
void actionCalibrate();
void actionSystemInfo();
void actionSettings();
void actionBackToMain();

// Main menu items
static const MenuItem MAIN_MENU[] = {
  {"Auto Track",    actionAutoTrack},
  {"Manual Control",actionManualControl},
  {"Calibrate LDR", actionCalibrate},
  {"System Info",   actionSystemInfo},
  {"Deep Sleep",    actionDeepSleep},
  {"Settings",      actionSettings},
};
static constexpr uint8_t MAIN_MENU_COUNT = 6;

// Settings submenu items
static const MenuItem SETTINGS_MENU[] = {
  {"LDR Threshold", nullptr},
  {"Deadzone",      nullptr},
  {"Night Thresh",  nullptr},
  {"Back",          actionBackToMain},
};
static constexpr uint8_t SETTINGS_MENU_COUNT = 4;

// Menu navigation state
enum MenuContext { MENU_NONE, MENU_MAIN, MENU_SETTINGS };
MenuContext menuContext = MENU_NONE;
uint8_t menuIndex = 0;
uint8_t menuMaxIndex = 0;
const MenuItem *currentMenu = MAIN_MENU;

// ─── Animation System ────────────────────────────────────────
struct Animation {
  uint8_t frame;
  uint8_t maxFrame;
  uint32_t interval;
  uint32_t lastFrame;
  bool playing;
};

Animation bootAnim = {0, 20, 80, 0, false};
Animation menuAnim = {0, 6, 50, 0, false};
Animation sunAnim  = {0, 0, 200, 0, true};
float sunPulse = 0.0f;

// ─── Tone System (Active Buzzer) ─────────────────────────────
// Defined here so it's available for playTone and constants
struct TonePattern {
  const char *pattern;
  uint8_t tempoMs;
};

// Tone constants
static constexpr TonePattern TONE_STARTUP = {"101011", 120};
static constexpr TonePattern TONE_SCROLL  = {"1", 60};
static constexpr TonePattern TONE_SELECT  = {"101", 100};
static constexpr TonePattern TONE_BACK    = {"1101", 120};
static constexpr TonePattern TONE_NIGHT   = {"10101000", 150};
static constexpr TonePattern TONE_ERROR   = {"11111", 60};

// Tone player state
struct TonePlayer {
  const char *pattern;
  uint8_t tempoMs;
  uint8_t index;
  uint32_t lastBeat;
  bool playing;
};
TonePlayer tonePlayer = {nullptr, 100, 0, 0, false};

// ─── Helper: Play Tone ───────────────────────────────────────
void playTone(const TonePattern &tone) {
  tonePlayer.pattern = tone.pattern;
  tonePlayer.tempoMs = tone.tempoMs;
  tonePlayer.index = 0;
  tonePlayer.lastBeat = millis();
  tonePlayer.playing = true;
}

void updateTonePlayer() {
  if (!tonePlayer.playing || !tonePlayer.pattern) return;
  uint32_t now = millis();
  if (now - tonePlayer.lastBeat >= tonePlayer.tempoMs) {
    tonePlayer.lastBeat = now;
    char beat = tonePlayer.pattern[tonePlayer.index];
    if (beat == '\0') {
      digitalWrite(PIN_BUZZER, LOW);
      tonePlayer.playing = false;
      return;
    }
    digitalWrite(PIN_BUZZER, (beat == '1') ? HIGH : LOW);
    tonePlayer.index++;
  }
}

// ─── Helper: Draw Filled Bar ─────────────────────────────────
void drawBar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t value, uint16_t maxVal) {
  uint8_t fillW = map(value, 0, maxVal, 0, w);
  display.drawRect(x, y, w, h, SSD1306_WHITE);
  display.fillRect(x, y, fillW, h, SSD1306_WHITE);
}

// ─── Helper: Draw Sun Icon ───────────────────────────────────
void drawSun(int8_t cx, int8_t cy, uint8_t radius) {
  display.fillCircle(cx, cy, radius, SSD1306_WHITE);
  static const int8_t rayOffsets[][2] = {
    {0, -1}, {1, -1}, {1, 0}, {1, 1},
    {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}
  };
  for (uint8_t i = 0; i < 8; i++) {
    int8_t dx = rayOffsets[i][0] * (radius + 2);
    int8_t dy = rayOffsets[i][1] * (radius + 2);
    display.drawLine(cx + dx/2, cy + dy/2, cx + dx, cy + dy, SSD1306_WHITE);
  }
}

// ─── Helper: Draw Animated Boot Sequence ─────────────────────
void drawBootAnimation() {
  display.clearDisplay();
  uint8_t f = bootAnim.frame;

  if (f <= 5) {
    int8_t sunY = 50 - f * 6;
    drawSun(64, sunY, 5);
    display.setTextSize(1);
    display.setCursor(20, 58);
    display.print("Sun Tracker");
  } else if (f <= 10) {
    drawSun(64, 20, 5 + (f - 5));
    display.setTextSize(1);
    display.setCursor(20, 58);
    display.print("Sun Tracker");
    for (uint8_t i = 0; i < (f - 5); i++) {
      int8_t rayLen = i * 2;
      for (uint8_t r = 0; r < 8; r++) {
        static const int8_t dirs[][2] = {
          {0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}
        };
        int8_t rx = 64 + dirs[r][0] * (7 + rayLen);
        int8_t ry = 20 + dirs[r][1] * (7 + rayLen);
        display.drawLine(64 + dirs[r][0]*7, 20 + dirs[r][1]*7, rx, ry, SSD1306_WHITE);
      }
    }
  } else if (f <= 15) {
    drawSun(64, 20, 10);
    display.setCursor(20, 58);
    display.print("Sun Tracker");
    uint8_t barW = (f - 10) * 20;
    display.fillRect(14, 38, barW, 4, SSD1306_WHITE);
    display.drawRect(14, 38, 100, 4, SSD1306_WHITE);
  } else {
    drawSun(64, 20, 10);
    display.setCursor(20, 58);
    display.print("Sun Tracker");
    display.fillRect(14, 38, 100, 4, SSD1306_WHITE);
    display.drawRect(14, 38, 100, 4, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(42, 48);
    if ((f - 15) % 2 == 0) display.print("Ready!");
  }
  display.display();
}

void updateBootAnimation() {
  if (!bootAnim.playing) return;
  uint32_t now = millis();
  if (now - bootAnim.lastFrame >= bootAnim.interval) {
    bootAnim.lastFrame = now;
    bootAnim.frame++;
    if (bootAnim.frame > bootAnim.maxFrame) {
      bootAnim.playing = false;
      bootAnim.frame = 0;
    }
  }
}

void updateMenuAnimation() {
  if (!menuAnim.playing) return;
  uint32_t now = millis();
  if (now - menuAnim.lastFrame >= menuAnim.interval) {
    menuAnim.lastFrame = now;
    menuAnim.frame++;
    if (menuAnim.frame >= menuAnim.maxFrame) {
      menuAnim.playing = false;
      menuAnim.frame = 0;
    }
  }
}

void updateSunPulse() {
  uint32_t now = millis();
  if (now - sunAnim.lastFrame >= sunAnim.interval) {
    sunAnim.lastFrame = now;
    sunPulse += 0.1f;
    if (sunPulse > 3.14159f * 2) sunPulse = 0;
  }
}

// ─── Helper: Draw Main Display ───────────────────────────────
void updateDisplayNormal() {
  display.clearDisplay();

  float pulse = (sin(sunPulse) + 1.0f) / 2.0f;
  uint8_t sunRadius = 4 + (uint8_t)(pulse * 2);
  int8_t sunX = 64;
  int8_t sunY = 10;

  int16_t diff = static_cast<int16_t>((ldrLeft + LDR_OFFSET) - ldrRight);
  sunX += map(constrain(diff, -200, 200), -200, 200, -15, 15);

  drawSun(sunX, sunY, sunRadius);

  const char *modeStr = (currentMode == MODE_AUTO)   ? "AUTO"
                        : (currentMode == MODE_MANUAL) ? "MAN"
                                                        : "STBY";
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(modeStr);

  display.setCursor(0, 16);
  display.print("L");
  drawBar(10, 16, 45, 6, ldrLeft, 4095);

  display.setCursor(0, 26);
  display.print("R");
  drawBar(10, 26, 45, 6, ldrRight, 4095);

  display.setCursor(60, 16);
  display.printf("%4u", ldrLeft);
  display.setCursor(60, 26);
  display.printf("%4u", ldrRight);

  display.setCursor(0, 38);
  if (abs(diff) < LDR_DEADZONE) {
    display.print("[=] CENTERED");
  } else if (diff > 0) {
    display.print("[>] TRACKING >>");
  } else {
    display.print("[<] TRACKING <<");
  }

  display.setCursor(0, 48);
  if (currentMode == MODE_STANDBY) {
    display.print("Servo: [■] STOP");
  } else if (diff > LDR_THRESHOLD) {
    display.print("Servo: [>>] CW");
  } else if (diff < -LDR_THRESHOLD) {
    display.print("Servo: [<<] CCW");
  } else {
    display.print("Servo: [■] HOLD");
  }

  display.setCursor(0, 58);
  display.print("[SELECT] Menu");

  display.display();
}

// ─── Helper: Draw Menu ──────────────────────────────────────
void updateDisplayMenu() {
  display.clearDisplay();

  uint8_t slideOffset = 0;
  if (menuAnim.playing) {
    slideOffset = map(menuAnim.frame, 0, menuAnim.maxFrame, 20, 0);
  }

  const char *title = (menuContext == MENU_MAIN) ? "MENU" : "SETTINGS";
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(slideOffset, 0);
  display.printf("== %s ==", title);
  display.drawFastHLine(slideOffset, 10, 128 - slideOffset, SSD1306_WHITE);

  uint8_t visibleItems = 4;
  uint8_t startIdx = 0;
  if (menuMaxIndex > visibleItems) {
    if (menuIndex >= visibleItems / 2 && menuIndex < menuMaxIndex - visibleItems / 2) {
      startIdx = menuIndex - visibleItems / 2;
    } else if (menuIndex >= menuMaxIndex - visibleItems / 2) {
      startIdx = menuMaxIndex - visibleItems;
    }
  }

  uint8_t endIdx = (startIdx + visibleItems > menuMaxIndex) ? menuMaxIndex : startIdx + visibleItems;

  for (uint8_t i = startIdx; i < endIdx; i++) {
    uint8_t itemDelay = (i - startIdx) * 1;
    int8_t itemX = slideOffset;
    if (menuAnim.playing && menuAnim.frame < (itemDelay + 3)) {
      itemX = 20 - menuAnim.frame * 3;
      if (itemX < 0) itemX = 0;
    }

    display.setCursor(itemX, 16 + (i - startIdx) * 10);
    if (i == menuIndex) {
      display.fillRect(itemX - 1, 14 + (i - startIdx) * 10, 128 - itemX, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.printf(" %s", currentMenu[i].label);
      display.setTextColor(SSD1306_WHITE);
    } else {
      display.printf("  %s", currentMenu[i].label);
    }
  }

  display.setCursor(slideOffset, 58);
  display.print("[L/R] Nav  [SEL] Select");
  display.display();
}

// ─── Helper: Update OLED ────────────────────────────────────
void updateDisplay() {
  if (menuContext == MENU_NONE) {
    updateDisplayNormal();
  } else {
    updateDisplayMenu();
  }
}

// ─── Helper: Open/Close Menu ────────────────────────────────
void openMenu(const MenuItem *menu, uint8_t count) {
  currentMenu = menu;
  menuMaxIndex = count;
  menuIndex = 0;
  menuContext = (menu == SETTINGS_MENU) ? MENU_SETTINGS : MENU_MAIN;
  menuAnim.playing = true;
  menuAnim.frame = 0;
}

void closeMenu() {
  menuContext = MENU_NONE;
  menuIndex = 0;
}

// ─── Helper: Read LDR ───────────────────────────────────────
uint16_t readLDR(uint8_t pin) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < LDR_SAMPLES; i++) {
    sum += analogRead(pin);
  }
  return static_cast<uint16_t>(sum / LDR_SAMPLES);
}

// ─── Helper: Debounced Button ───────────────────────────────
bool readButton(uint8_t pin, uint8_t btnIndex) {
  bool raw = digitalRead(pin) == LOW;
  uint32_t now = millis();
  if (raw != btnStates[btnIndex].lastRaw) {
    btnStates[btnIndex].lastChange = now;
  }
  btnStates[btnIndex].lastRaw = raw;
  if (raw && (now - btnStates[btnIndex].lastChange >= DEBOUNCE_MS)) {
    if (!btnStates[btnIndex].pressed) {
      btnStates[btnIndex].pressed = true;
      return true;
    }
  } else if (!raw) {
    btnStates[btnIndex].pressed = false;
  }
  return false;
}

// ─── Helper: Handle LEFT Button ─────────────────────────────
void handleLeftButton() {
  if (readButton(PIN_BTN_LEFT, 0)) {
    if (menuContext != MENU_NONE) {
      if (menuIndex > 0) {
        menuIndex--;
        playTone(TONE_SCROLL);
      }
    } else {
      if (currentMode == MODE_MANUAL || currentMode == MODE_STANDBY) {
        sunServo.write(SERVO_CCW_SLOW);
        playTone(TONE_SCROLL);
      }
    }
  }
}

// ─── Helper: Handle RIGHT Button ────────────────────────────
void handleRightButton() {
  if (readButton(PIN_BTN_RIGHT, 1)) {
    if (menuContext != MENU_NONE) {
      if (menuIndex < menuMaxIndex - 1) {
        menuIndex++;
        playTone(TONE_SCROLL);
      }
    } else {
      if (currentMode == MODE_MANUAL || currentMode == MODE_STANDBY) {
        sunServo.write(SERVO_CW_SLOW);
        playTone(TONE_SCROLL);
      }
    }
  }
}

// ─── Helper: Handle SELECT Button ───────────────────────────
void handleSelectButton() {
  if (readButton(PIN_BTN_SELECT, 2)) {
    if (menuContext != MENU_NONE) {
      if (currentMenu[menuIndex].action != nullptr) {
        currentMenu[menuIndex].action();
        playTone(TONE_SELECT);
      } else {
        playTone(TONE_ERROR);
      }
    } else {
      openMenu(MAIN_MENU, MAIN_MENU_COUNT);
      playTone(TONE_SELECT);
    }
  }
}

// ─── Menu Actions ────────────────────────────────────────────
void actionAutoTrack() {
  currentMode = MODE_AUTO;
  currentState = STATE_ACTIVE;
  closeMenu();
}

void actionManualControl() {
  currentMode = MODE_MANUAL;
  currentState = STATE_ACTIVE;
  closeMenu();
}

void actionDeepSleep() {
  currentMode = MODE_STANDBY;
  currentState = STATE_FORCED_SLEEP;
  closeMenu();
  goToSleep();
}

void actionCalibrate() {
  closeMenu();
}

void actionSystemInfo() {
  closeMenu();
}

void actionSettings() {
  openMenu(SETTINGS_MENU, SETTINGS_MENU_COUNT);
}

void actionBackToMain() {
  openMenu(MAIN_MENU, MAIN_MENU_COUNT);
}

// ─── Helper: Sun Tracking Logic ─────────────────────────────
void trackSun() {
  if (currentMode != MODE_AUTO) return;
  int16_t diff = static_cast<int16_t>((ldrLeft + LDR_OFFSET) - ldrRight);
  if (abs(diff) < static_cast<int16_t>(LDR_DEADZONE)) {
    sunServo.write(SERVO_STOP_REAL);
  } else if (diff > static_cast<int16_t>(LDR_THRESHOLD)) {
    sunServo.write(SERVO_CW_SLOW);
  } else if (diff < -static_cast<int16_t>(LDR_THRESHOLD)) {
    sunServo.write(SERVO_CCW_SLOW);
  }
}

bool isNight() {
  return (ldrLeft < NIGHT_THRESHOLD) && (ldrRight < NIGHT_THRESHOLD);
}

// ─── Helper: Enter Deep Sleep ────────────────────────────────
void goToSleep() {
  playTone(TONE_NIGHT);
  delay(800);

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 20);
  display.print("SLEEP");
  display.display();

  digitalWrite(PIN_BUZZER, LOW);
  sunServo.detach();

  esp_sleep_enable_timer_wakeup(
      static_cast<uint64_t>(SLEEP_CHECK_INTERVAL) * 60ULL * 1000000ULL);
  esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(PIN_BTN_WAKE), LOW);

  delay(500);
  esp_deep_sleep_start();
}

// ─── Setup ──────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Sun Tracker ===");
  Serial.println("Booting...");

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
  pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_BTN_SELECT, INPUT_PULLUP);

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("ERROR: OLED init failed!");
    while (true);
  }

  Serial.println("OLED ready. Playing boot animation...");
  bootAnim.playing = true;
  bootAnim.frame = 1;
  while (bootAnim.playing) {
    updateBootAnimation();
    delay(10);
  }

  sunServo.attach(PIN_SERVO);
  sunServo.write(SERVO_STOP_REAL);
  Serial.println("Servo ready.");

  ldrLeft = readLDR(PIN_LDR_LEFT);
  ldrRight = readLDR(PIN_LDR_RIGHT);
  Serial.printf("LDR — Left: %u | Right: %u\n", ldrLeft, ldrRight);

  // DISABLED FOR INDOOR TESTING
  if (false && isNight()) {
    Serial.println("No sunlight detected — entering deep sleep.");
    currentState = STATE_NIGHT_SLEEP;
    goToSleep();
  }

  playTone(TONE_STARTUP);
  Serial.println("=== Setup Complete ===");
}

// ─── Main Loop ───────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  updateTonePlayer();
  updateSunPulse();
  updateMenuAnimation();

  if (now - lastLDRRead >= INTERVAL_LDR) {
    lastLDRRead = now;

    // Raw reading (16x oversampled)
    uint16_t rawLeft = readLDR(PIN_LDR_LEFT);
    uint16_t rawRight = readLDR(PIN_LDR_RIGHT);

    // Rolling average (smooths remaining noise)
    ldrLeftBuf[ldrBufIndex] = rawLeft;
    ldrRightBuf[ldrBufIndex] = rawRight;
    ldrBufIndex = (ldrBufIndex + 1) % LDR_AVG_WINDOW;

    uint32_t sumLeft = 0, sumRight = 0;
    for (uint8_t i = 0; i < LDR_AVG_WINDOW; i++) {
      sumLeft += ldrLeftBuf[i];
      sumRight += ldrRightBuf[i];
    }
    ldrLeft = sumLeft / LDR_AVG_WINDOW;
    ldrRight = sumRight / LDR_AVG_WINDOW;

    Serial.printf("L: %4u | R: %4u | RawDiff: %+d | Action: ",
                  ldrLeft, ldrRight,
                  static_cast<int16_t>(ldrLeft - ldrRight));

    // Apply offset to balance LDR manufacturing differences
    int16_t diff = static_cast<int16_t>((ldrLeft + LDR_OFFSET) - ldrRight);

    Serial.printf("AdjDiff: %+d | ", diff);
    if (abs(diff) < static_cast<int16_t>(LDR_DEADZONE)) {
      Serial.println("HOLD (90)");
    } else if (diff > LDR_THRESHOLD) {
      Serial.println("MOVE CW");
    } else if (diff < -LDR_THRESHOLD) {
      Serial.println("MOVE CCW");
    } else {
      Serial.println("WAIT");
    }
  }

  // DISABLED FOR INDOOR TESTING
  if (currentState == STATE_ACTIVE && false && isNight()) {
    Serial.println("Night detected — entering deep sleep.");
    currentState = STATE_NIGHT_SLEEP;
    goToSleep();
  }

  if (now - lastButtonCheck >= INTERVAL_BUTTONS) {
    lastButtonCheck = now;
    handleLeftButton();
    handleRightButton();
    handleSelectButton();
  }

  if (now - lastServoUpdate >= SERVO_UPDATE_MS) {
    lastServoUpdate = now;
    if (currentMode == MODE_AUTO) {
      trackSun();
    } else {
      sunServo.write(SERVO_STOP_REAL);
    }
  }

  if (now - lastDisplayUpdate >= INTERVAL_DISPLAY) {
    lastDisplayUpdate = now;
    updateDisplay();
  }
}
