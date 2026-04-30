#include <BluetoothSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

// ── OLED ──
static constexpr uint8_t OLED_SDA = 21;
static constexpr uint8_t OLED_SCL = 22;
static constexpr int16_t SCREEN_W = 128;
static constexpr int16_t SCREEN_H = 64;
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

// ── Servo ──
static constexpr uint8_t PIN_SERVO = 12;
Servo servo;

// ── LDR ──
static constexpr uint8_t PIN_LDR_L = 34;
static constexpr uint8_t PIN_LDR_R = 35;

// ── Limiters ──
static constexpr uint8_t PIN_LIM_L = 15;
static constexpr uint8_t PIN_LIM_R = 13;

// ── Buttons ──
static constexpr uint8_t PIN_BTN_L = 4;
static constexpr uint8_t PIN_BTN_R = 19;
static constexpr uint8_t PIN_BTN_S = 18;

// ── Battery ──
static constexpr uint8_t PIN_BATT = 36;

// ── Buzzer ──
static constexpr uint8_t PIN_BUZZ = 14;

  // ── Constants ──
static constexpr int LDR_THRESH = 100;
static constexpr int IDLE_MS = 30000;
static constexpr int SLEEP_CHECK_MS = 30000;
static constexpr int PULSE_MS = 20;
static constexpr int PULSE_WAIT_MS = 0;
static constexpr int MENU_DEBOUNCE_MS = 200;
static constexpr int BTN_DEBOUNCE_MS = 50;
static constexpr int WRAP_ESCAPE_PULSES = 5;
static constexpr int WRAP_ESCAPE_MS = 10;
static constexpr int LDR_SAMPLES = 1024;
static constexpr int BT_TELEM_MS = 1000;
static constexpr float BATT_DIVIDER = 2.0f;   // voltage divider ratio
static constexpr float BATT_V_MAX = 4.2f;     // battery full
static constexpr float BATT_V_MIN = 3.35f;    // battery empty

BluetoothSerial SerialBT;

// ── LUT Battery (11 points: 3.0V → 4.2V) ──
// LUT: 11 points, BATT_V_MIN → BATT_V_MAX, nonlinear Li-ion curve
static constexpr int BATT_LUT[] = {0, 0, 5, 10, 15, 20, 30, 55, 75, 90, 100};

// ── Enums ──
enum class Mode { AUTO,
                  MANUAL,
                  SLEEP,
                  WRAP };
enum class Dir { NONE,
                 LEFT,
                 RIGHT };

// ── Globals ──
Mode currentMode = Mode::AUTO;
int leftLDR = 0, rightLDR = 0;
int batteryPct = 0;
int batteryRaw = 0;
float batteryVolt = 0.0f;
int stuckCount = 0;
Dir lastDir = Dir::NONE;
bool leftLimit = false, rightLimit = false;
unsigned long idleTimer = 0;
unsigned long sleepCheckTimer = 0;
unsigned long btTelemTimer = 0;
bool sleepToneDone = false;
unsigned long btnLastTime[3] = {0, 0, 0};
bool btnLastState[3] = {false, false, false};
bool btnReady[3] = {true, true, true};

// ── Forward Declarations ──
void playBootTone();
void playMenuTone();
void playButtonTone();
void playSleepTone();
void playWakeTone();
void handleSleep();
void readLDRs();
void readBattery();
void updateDisplay();
void menu();
void autoMode();
void manualMode();
void wrapMode();
void executeWrap(Dir blockedDir);
void pulseServo(Dir dir);
void readLimiters();
bool debouncedRead(int pin);
bool limiterTouched();
Dir towardLimiter();
bool btnPressed(int pin);
bool btnHeld(int pin);
bool checkSelectButton();
bool anyButtonPressed();
void displayMenu(const char* selected);
void displaySleep();
int btnIndex(int pin);
void checkBluetooth();
void sendTelemetry();

// ── Setup ──
void setup() {
    Serial.begin(115200);
  analogSetAttenuation(ADC_11db);
  SerialBT.begin("SolarTracker");
    pinMode(PIN_LIM_L, INPUT_PULLUP);
    pinMode(PIN_LIM_R, INPUT_PULLUP);
    pinMode(PIN_BTN_L, INPUT_PULLUP);
    pinMode(PIN_BTN_R, INPUT_PULLUP);
    pinMode(PIN_BTN_S, INPUT_PULLUP);
    pinMode(PIN_BUZZ, OUTPUT);

    servo.attach(PIN_SERVO);
    servo.write(90); // stop

  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  playBootTone();
  idleTimer = millis();
}

// ── Main Loop ──
void loop() {
  if (currentMode == Mode::SLEEP) {
    handleSleep();
    return;
  }

  sleepToneDone = false;
  readLDRs();
  readBattery();
  checkBluetooth();
  updateDisplay();

  Serial.print("L:"); Serial.print(leftLDR);
  Serial.print(" R:"); Serial.print(rightLDR);
  Serial.print(" diff:"); Serial.print(leftLDR - rightLDR);
  Serial.print(" LL:"); Serial.print(leftLimit ? "T" : "-");
  Serial.print(" RL:"); Serial.print(rightLimit ? "T" : "-");
  Serial.print(" Bat:"); Serial.print(batteryPct);
  Serial.print("%("); Serial.print(batteryRaw);
  Serial.print(","); Serial.print(batteryVolt, 2);
  Serial.print("V) M:");
  switch (currentMode) {
    case Mode::AUTO: Serial.print("AUTO"); break;
    case Mode::MANUAL: Serial.print("MAN"); break;
    case Mode::SLEEP: Serial.print("SLP"); break;
    case Mode::WRAP: Serial.print("WRP"); break;
  }
  Serial.println();

  if (checkSelectButton()) {
    playMenuTone();
    menu();
    return;
  }

  switch (currentMode) {
    case Mode::AUTO: autoMode(); break;
    case Mode::MANUAL: manualMode(); break;
    case Mode::WRAP: wrapMode(); break;
    default: break;
  }
}

// ── Menu (blocking) ──
void menu() {
  const char* names[] = { "AUTO", "MANUAL", "SLEEP" };
  int idx = static_cast<int>(currentMode);

  while (true) {
    displayMenu(names[idx]);

    if (btnPressed(PIN_BTN_L)) {
      playButtonTone();
      idx = (idx + 2) % 3;
      delay(MENU_DEBOUNCE_MS);
    }
    if (btnPressed(PIN_BTN_R)) {
      playButtonTone();
      idx = (idx + 1) % 3;
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

// ── Manual Mode ──
void manualMode() {
  Dir dir = Dir::NONE;
  if (btnHeld(PIN_BTN_L)) dir = Dir::LEFT;
  else if (btnHeld(PIN_BTN_R)) dir = Dir::RIGHT;

  if (dir == Dir::NONE) return;

  readLimiters();

  if (limiterTouched()) {
    if (dir == towardLimiter()) {
      Serial.println("BLOCKED: moving into limiter");
      return;
    }
  }

  Serial.print("PULSE MANUAL: ");
  Serial.println(dir == Dir::LEFT ? "LEFT" : "RIGHT");
  pulseServo(dir);
  delay(PULSE_WAIT_MS);
}

// ── Auto Mode ──
void autoMode() {
  int diff = leftLDR - rightLDR;

  if (abs(diff) < LDR_THRESH) {
    if (millis() - idleTimer > IDLE_MS) {
      currentMode = Mode::SLEEP;
    }
    return;
  }

  idleTimer = millis();
  readLimiters();

  Dir want = (diff > 0) ? Dir::LEFT : Dir::RIGHT;

  if (!limiterTouched()) {
    stuckCount = 0;
    Serial.print("PULSE AUTO: "); Serial.println(want == Dir::LEFT ? "LEFT" : "RIGHT");
    pulseServo(want);
    return;
  }

  if (want != towardLimiter()) {
    stuckCount = 0;
    Serial.print("PULSE AUTO (away): "); Serial.println(want == Dir::LEFT ? "LEFT" : "RIGHT");
    pulseServo(want);
    return;
  }

  // moving into limiter
  if (stuckCount < 5) {
    stuckCount++;
    Serial.print("BLOCKED stuckCount="); Serial.println(stuckCount);
    return;
  }

  Serial.println("WRAP!");
  currentMode = Mode::WRAP;
  executeWrap(want);
  currentMode = Mode::AUTO;
  stuckCount = 0;
}

// ── Wrap Mode ──
void wrapMode() {}  // logic handled inside autoMode executeWrap

void executeWrap(Dir blockedDir) {
  Dir escapeDir = (blockedDir == Dir::LEFT) ? Dir::RIGHT : Dir::LEFT;

  Serial.print("WRAP: rotating "); Serial.println(escapeDir == Dir::LEFT ? "LEFT" : "RIGHT");

  // continuous rotation until other limiter touched
  int val = (escapeDir == Dir::LEFT) ? 0 : 180;
  servo.write(val);

  unsigned long start = millis();
  while (millis() - start < 10000) {
    readLimiters();
    if (escapeDir == Dir::LEFT && leftLimit) {
      Serial.println("WRAP: hit left limiter");
      break;
    }
    if (escapeDir == Dir::RIGHT && rightLimit) {
      Serial.println("WRAP: hit right limiter");
      break;
    }
    delay(10);
  }

  servo.write(90); // stop

  Serial.print("WRAP: escaping "); Serial.print(WRAP_ESCAPE_PULSES);
  Serial.println(" pulses");
  for (int i = 0; i < WRAP_ESCAPE_PULSES; i++) {
    int val = (blockedDir == Dir::LEFT) ? 0 : 180;
    servo.write(val);
    delay(WRAP_ESCAPE_MS);
    servo.write(90);
    delay(PULSE_WAIT_MS);
  }

  Serial.println("WRAP: done");
}

// ── Sleep Mode ──
void handleSleep() {
  if (!sleepToneDone) {
    playSleepTone();
    sleepToneDone = true;
  }

  displaySleep();

  unsigned long now = millis();

  if (now - sleepCheckTimer >= SLEEP_CHECK_MS) {
    sleepCheckTimer = now;
    readLDRs();
    if (abs(leftLDR - rightLDR) >= LDR_THRESH) {
      Serial.println("WAKE: LDR diff");
      playWakeTone();
  sleepToneDone = false;
  readLDRs();
      currentMode = Mode::AUTO;
      return;
    }
  }

  if (digitalRead(PIN_BTN_L) == LOW || digitalRead(PIN_BTN_R) == LOW || digitalRead(PIN_BTN_S) == LOW) {
    Serial.println("WAKE: button");
    playWakeTone();
    sleepToneDone = false;
    currentMode = Mode::AUTO;
    return;
  }

  esp_sleep_enable_timer_wakeup(500000ULL);
  esp_light_sleep_start();
}

// ── Servo Pulse ──
void pulseServo(Dir dir) {
  int val = (dir == Dir::LEFT) ? 0 : 180;
  servo.write(val);
  delay(PULSE_MS);
  servo.write(90);  // stop
  lastDir = dir;
}

// ── Sensors ──
void readLDRs() {
  long sumL = 0, sumR = 0;
  for (int i = 0; i < LDR_SAMPLES; i++) {
    sumL += analogRead(PIN_LDR_L);
    sumR += analogRead(PIN_LDR_R);
  }
  leftLDR = static_cast<int>(sumL / LDR_SAMPLES);
  rightLDR = static_cast<int>(sumR / LDR_SAMPLES);
}

void readLimiters() {
  leftLimit = debouncedRead(PIN_LIM_L);
  rightLimit = debouncedRead(PIN_LIM_R);
}

bool debouncedRead(int pin) {
  int count = 0;
  for (int i = 0; i < 5; i++) {
    if (digitalRead(pin) == LOW) count++;
    delay(1);
  }
  return count >= 4; // 4 of 5 reads LOW = pressed
}

void readBattery() {
  batteryRaw = analogRead(PIN_BATT);
  batteryVolt = batteryRaw * (3.3f / 4095.0f) * BATT_DIVIDER;
  float frac = (batteryVolt - BATT_V_MIN) / (BATT_V_MAX - BATT_V_MIN);
  int idx = constrain((int)(frac * 10.0f), 0, 10);
  batteryPct = BATT_LUT[idx];
}

// ── Helpers ──
bool limiterTouched() {
  return leftLimit || rightLimit;
}

Dir towardLimiter() {
  if (leftLimit) return Dir::LEFT;
  if (rightLimit) return Dir::RIGHT;
  return Dir::NONE;
}

int btnIndex(int pin) {
  if (pin == PIN_BTN_L) return 0;
  if (pin == PIN_BTN_R) return 1;
  return 2; // PIN_BTN_S
}

bool btnPressed(int pin) {
  int idx = btnIndex(pin);
  bool reading = digitalRead(pin) == LOW;
  unsigned long now = millis();

  if (reading != btnLastState[idx]) {
    btnLastTime[idx] = now;
    btnLastState[idx] = reading;
    return false;
  }

  if ((now - btnLastTime[idx]) > BTN_DEBOUNCE_MS) {
    if (reading && btnReady[idx]) {
      btnReady[idx] = false;
      return true;
    }
    if (!reading) {
      btnReady[idx] = true;
    }
  }
  return false;
}

bool btnHeld(int pin) {
  return digitalRead(pin) == LOW;
}

bool checkSelectButton() {
  return btnPressed(PIN_BTN_S);
}

bool anyButtonPressed() {
  return btnPressed(PIN_BTN_L) || btnPressed(PIN_BTN_R) || btnPressed(PIN_BTN_S);
}

void checkBluetooth() {
  if (millis() - btTelemTimer >= BT_TELEM_MS) {
    btTelemTimer = millis();
    sendTelemetry();
  }

  while (SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "A" || cmd == "AUTO") {
      currentMode = Mode::AUTO;
      SerialBT.println("OK AUTO");
    } else if (cmd == "M" || cmd == "MANUAL") {
      currentMode = Mode::MANUAL;
      SerialBT.println("OK MANUAL");
    } else if (cmd == "S" || cmd == "SLEEP") {
      currentMode = Mode::SLEEP;
      SerialBT.println("OK SLEEP");
    } else if (cmd == "L" || cmd == "LEFT") {
      if (currentMode == Mode::MANUAL) {
        pulseServo(Dir::LEFT);
        SerialBT.println("OK LEFT");
      } else {
        SerialBT.println("ERR: not in MANUAL");
      }
    } else if (cmd == "R" || cmd == "RIGHT") {
      if (currentMode == Mode::MANUAL) {
        pulseServo(Dir::RIGHT);
        SerialBT.println("OK RIGHT");
      } else {
        SerialBT.println("ERR: not in MANUAL");
      }
    } else if (cmd == "?" || cmd == "STATUS") {
      sendTelemetry();
    } else if (cmd.length() > 0) {
      SerialBT.println("ERR: unknown cmd");
    }
  }
}

void sendTelemetry() {
  int diff = leftLDR - rightLDR;
  const char* dir = (abs(diff) < LDR_THRESH) ? "CENTER" : ((diff > 0) ? "LEFT" : "RIGHT");
  const char* mode = (currentMode == Mode::AUTO) ? "AUTO" :
                     (currentMode == Mode::MANUAL) ? "MANUAL" :
                     (currentMode == Mode::SLEEP) ? "SLEEP" : "WRAP";

  SerialBT.print("MODE:"); SerialBT.print(mode);
  SerialBT.print(" B:"); SerialBT.print(batteryPct);
  SerialBT.print(" L:"); SerialBT.print(leftLDR);
  SerialBT.print(" R:"); SerialBT.print(rightLDR);
  SerialBT.print(" D:"); SerialBT.println(dir);
}

// ── Display ──
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  const char* modeName = (currentMode == Mode::AUTO) ? "AUTO" :
                         (currentMode == Mode::MANUAL) ? "MANUAL" :
                         (currentMode == Mode::SLEEP) ? "SLEEP" : "WRAP";

  display.print("MODE:"); display.println(modeName);
  display.println();
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

// ── Buzzer ──
void playBootTone() {
  tone(PIN_BUZZ, 523, 200); delay(250);
  tone(PIN_BUZZ, 659, 200); delay(250);
  tone(PIN_BUZZ, 784, 200); delay(250);
  noTone(PIN_BUZZ);
}

void playMenuTone() {
  tone(PIN_BUZZ, 660, 80); delay(100);
  tone(PIN_BUZZ, 880, 80); delay(100);
  noTone(PIN_BUZZ);
}

void playButtonTone() {
  tone(PIN_BUZZ, 1000, 30);
}

void playSleepTone() {
  tone(PIN_BUZZ, 660, 100); delay(150);
  tone(PIN_BUZZ, 440, 200); delay(250);
  noTone(PIN_BUZZ);
}

void playWakeTone() {
  tone(PIN_BUZZ, 440, 100); delay(150);
  tone(PIN_BUZZ, 660, 150); delay(200);
  noTone(PIN_BUZZ);
}
