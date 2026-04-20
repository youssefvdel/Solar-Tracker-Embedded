/*
 * Solar Tracker - ESP32 Sun Tracking System
 * University Embedded Systems Project
 */

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <Wire.h>

// ============================================
// PIN CONNECTIONS
// ============================================
const uint8_t PIN_SERVO = 12;
const uint8_t PIN_LDR_LEFT = 34;
const uint8_t PIN_LDR_RIGHT = 35;
const uint8_t PIN_BUZZER = 14;
const uint8_t PIN_OLED_SDA = 21;
const uint8_t PIN_OLED_SCL = 22;
const uint8_t PIN_BTN_LEFT = 4;
const uint8_t PIN_BTN_SELECT = 18;
const uint8_t PIN_BTN_RIGHT = 19;
const uint8_t PIN_LIMIT_LEFT = 13;
const uint8_t PIN_LIMIT_RIGHT = 15;
const uint8_t PIN_BATTERY = 36;

// ============================================
// DISPLAY SETTINGS
// ============================================
const uint8_t SCREEN_W = 128;
const uint8_t SCREEN_H = 64;
const uint8_t OLED_ADDR = 0x3C;

// ============================================
// SERVO SETTINGS
// ============================================
const uint8_t SERVO_STOP = 90;
const uint8_t SERVO_CW = 0;
const uint8_t SERVO_CCW = 180;

// ============================================
// LDR SETTINGS
// ============================================
const uint16_t LDR_SAMPLES = 128;      // ADC samples per reading
const uint16_t LDR_DEADZONE = 200;     // Stop if diff < this
const int16_t LDR_OFFSET = 0;          // Sensor calibration offset
const int16_t LDR_HYSTERESIS = 80;     // Need this to reverse direction

// ============================================
// TIMING INTERVALS (ms)
// ============================================
const uint32_t TIME_LDR = 200;
const uint32_t TIME_DISPLAY = 100;
const uint32_t TIME_BUTTONS = 20;
const uint32_t DEBOUNCE = 50;

// ============================================
// POWER
// ============================================
const uint32_t IDLE_TIMEOUT_MIN = 1;
const uint32_t WAKE_INTERVAL_SEC = 30;

// ============================================
// SERVO PULSE
// ============================================
const uint32_t SERVO_ON = 15;
const uint32_t SERVO_OFF = 250;

// ============================================
// GLOBAL OBJECTS
// ============================================
Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, -1);
Servo trackerServo;

// ============================================
// SYSTEM STATE
// ============================================
enum SysMode { AUTO, MANUAL, STANDBY };
enum MenuState { NO_MENU, MAIN_MENU };

SysMode currentMode = AUTO;
MenuState menuState = NO_MENU;

// ============================================
// MENU
// ============================================
void setAutoMode();
void setManualMode();
void enterSleepMode();

struct MenuOption {
    const char* name;
    void (*action)();
};

const MenuOption mainMenu[] = {
    {"Auto", setAutoMode},
    {"Manual", setManualMode},
    {"Sleep", enterSleepMode}
};
const uint8_t MENU_COUNT = 3;
uint8_t menuPos = 0;
const MenuOption* activeMenu = nullptr;

// ============================================
// SENSORS
// ============================================
uint16_t ldrL = 0, ldrR = 0;
uint16_t bufL[5] = {0}, bufR[5] = {0};
uint8_t bufIdx = 0;
uint8_t battery = 0;

// ============================================
// TIMING
// ============================================
uint32_t tLDR = 0, tDisplay = 0, tBtn = 0, tBat = 0, tPulse = 0;
uint32_t tLastMove = 0;
bool pulseActive = false;

// ============================================
// BUTTONS
// ============================================
struct Button {
    bool pressed = false;
    bool lastState = false;
    uint32_t tChange = 0;
};
Button btn[3];

// ============================================
// BUZZER
// ============================================
const char BEEP_START[] = "101011";
const char BEEP_SCROLL[] = "1";
const char BEEP_SELECT[] = "101";
const char BEEP_SLEEP[] = "10101000";

struct ToneGen {
    const char* pattern = nullptr;
    uint8_t speed = 100;
    uint8_t index = 0;
    uint32_t tLast = 0;
    bool playing = false;
} beep;

// ============================================
// TRACKING STATE
// ============================================
bool lastDirection = false;
bool hasPulsed = false;

// ============================================
// FORWARD DECLARATIONS
// ============================================
void readSensors();
void updateBattery();
void checkButtons();
void trackingLogic();
uint16_t readSensor(uint8_t pin);
bool buttonPress(uint8_t pin, uint8_t idx);
void showDisplay();
void sleepMode();
void playTone(const char* pattern);

// ============================================
// SETUP
// ============================================
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Solar Tracker Starting ===");

    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);

    pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
    pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
    pinMode(PIN_BTN_SELECT, INPUT_PULLUP);
    pinMode(PIN_LIMIT_LEFT, INPUT_PULLUP);
    pinMode(PIN_LIMIT_RIGHT, INPUT_PULLUP);
    pinMode(PIN_BATTERY, INPUT);

    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("OLED Error!");
        while(1);
    }

    trackerServo.attach(PIN_SERVO);
    trackerServo.write(SERVO_STOP);

    ldrL = readSensor(PIN_LDR_LEFT);
    ldrR = readSensor(PIN_LDR_RIGHT);
    Serial.printf("Initial LDR: L=%u R=%u\n", ldrL, ldrR);

    tLastMove = millis();

    playTone(BEEP_START);
    Serial.println("Ready");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
    uint32_t now = millis();

    // Buzzer tone
    if (beep.playing && beep.pattern) {
        if (now - beep.tLast >= beep.speed) {
            beep.tLast = now;
            char bit = beep.pattern[beep.index];
            if (bit == '\0') {
                digitalWrite(PIN_BUZZER, LOW);
                beep.playing = false;
            } else {
                digitalWrite(PIN_BUZZER, bit == '1' ? HIGH : LOW);
                beep.index++;
            }
        }
    }

    // Servo pulse timing
    if (now - tPulse >= (pulseActive ? SERVO_ON : SERVO_OFF)) {
        tPulse = now;
        pulseActive = !pulseActive;
    }

    // Read LDR sensors
    if (now - tLDR >= TIME_LDR) {
        tLDR = now;
        readSensors();
        hasPulsed = false;  // Allow new pulse

        int16_t diff = (ldrL + LDR_OFFSET) - ldrR;
        Serial.printf("L:%u R:%u D:%d ", ldrL, ldrR, diff);
        Serial.println(abs(diff) < LDR_DEADZONE ? "CENTER" : (diff > 0 ? "LEFT" : "RIGHT"));
    }

    // Battery check
    if (now - tBat >= 1000) {
        tBat = now;
        updateBattery();
    }

    // Check buttons
    if (now - tBtn >= TIME_BUTTONS) {
        tBtn = now;
        checkButtons();
    }

    // Auto tracking
    if (currentMode == AUTO) {
        trackingLogic();
    }

    // IDLE SLEEP DISABLED FOR TESTING
    // if (currentMode == AUTO && (now - tLastMove) >= (IDLE_TIMEOUT_MIN * 60000UL)) {
    //     Serial.println("Idle timeout - sleeping");
    //     sleepMode();
    // }

    // Update display
    if (now - tDisplay >= TIME_DISPLAY) {
        tDisplay = now;
        showDisplay();
    }
}

// ============================================
// SENSOR FUNCTIONS
// ============================================
void readSensors() {
    uint16_t rawL = readSensor(PIN_LDR_LEFT);
    uint16_t rawR = readSensor(PIN_LDR_RIGHT);

    bufL[bufIdx] = rawL;
    bufR[bufIdx] = rawR;
    bufIdx = (bufIdx + 1) % 5;

    uint32_t sumL = 0, sumR = 0;
    for (uint8_t i = 0; i < 5; i++) {
        sumL += bufL[i];
        sumR += bufR[i];
    }
    ldrL = sumL / 5;
    ldrR = sumR / 5;
}

uint16_t readSensor(uint8_t pin) {
    uint32_t total = 0;
    for (uint16_t i = 0; i < LDR_SAMPLES; i++) {
        total += analogRead(pin);
    }
    return total / LDR_SAMPLES;
}

void updateBattery() {
    uint16_t raw = analogRead(PIN_BATTERY);
    float volts = (raw / 4095.0f) * 3.3f * 2.0f;
    int pct = (int)((volts - 3.0f) * 100.0f / 1.2f);
    battery = constrain(pct, 0, 100);
    Serial.printf("Bat: %u%%\n", battery);
}

// ============================================
// TRACKING
// ============================================
void trackingLogic() {
    // Check limit switches
    bool limitL = digitalRead(PIN_LIMIT_LEFT) == LOW;
    bool limitR = digitalRead(PIN_LIMIT_RIGHT) == LOW;

    if (limitL) {
        lastDirection = false;
        Serial.println("LIMIT LEFT");
    } else if (limitR) {
        lastDirection = true;
        Serial.println("LIMIT RIGHT");
    }

    int16_t diff = (ldrL + LDR_OFFSET) - ldrR;

    // Centered - stop and reset timer
    if (abs(diff) < LDR_DEADZONE) {
        trackerServo.write(SERVO_STOP);
        tLastMove = millis();
        return;
    }

    bool newDir = diff > 0;

    // Hysteresis check
    if (newDir != lastDirection && abs(diff) < LDR_HYSTERESIS) {
        trackerServo.write(SERVO_STOP);
        return;
    }
    lastDirection = newDir;

    // One pulse per cycle
    if (!hasPulsed && pulseActive) {
        hasPulsed = true;
        tLastMove = millis();
        trackerServo.write(lastDirection ? SERVO_CW : SERVO_CCW);
        delay(SERVO_ON);
        trackerServo.write(SERVO_STOP);
    }
}

// ============================================
// BUTTONS
// ============================================
bool buttonPress(uint8_t pin, uint8_t idx) {
    bool raw = digitalRead(pin) == LOW;
    uint32_t now = millis();
    if (raw != btn[idx].lastState) {
        btn[idx].tChange = now;
    }
    btn[idx].lastState = raw;

    if (raw && (now - btn[idx].tChange) >= DEBOUNCE) {
        if (!btn[idx].pressed) {
            btn[idx].pressed = true;
            return true;
        }
    } else if (!raw) {
        btn[idx].pressed = false;
    }
    return false;
}

void checkButtons() {
    if (buttonPress(PIN_BTN_LEFT, 0)) {
        if (menuState == MAIN_MENU && menuPos > 0) menuPos--;
        else if (currentMode == MANUAL && digitalRead(PIN_LIMIT_LEFT) != LOW) {
            trackerServo.write(SERVO_CCW);
        }
        playTone(BEEP_SCROLL);
    }

    if (buttonPress(PIN_BTN_RIGHT, 1)) {
        if (menuState == MAIN_MENU && menuPos < MENU_COUNT - 1) menuPos++;
        else if (currentMode == MANUAL && digitalRead(PIN_LIMIT_RIGHT) != LOW) {
            trackerServo.write(SERVO_CW);
        }
        playTone(BEEP_SCROLL);
    }

    if (buttonPress(PIN_BTN_SELECT, 2)) {
        if (menuState == MAIN_MENU && activeMenu && activeMenu[menuPos].action) {
            activeMenu[menuPos].action();
        } else {
            activeMenu = mainMenu;
            menuState = MAIN_MENU;
        }
        playTone(BEEP_SELECT);
    }
}

void playTone(const char* pattern) {
    beep.pattern = pattern;
    beep.index = 0;
    beep.tLast = millis();
    beep.playing = true;
}

// ============================================
// MENU ACTIONS
// ============================================
void setAutoMode() {
    currentMode = AUTO;
    menuState = NO_MENU;
}

void setManualMode() {
    currentMode = MANUAL;
    menuState = NO_MENU;
}

void enterSleepMode() {
    menuState = NO_MENU;
    sleepMode();
}

// ============================================
// DISPLAY
// ============================================
void showDisplay() {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);

    if (menuState == NO_MENU) {
        const char* modeStr = (currentMode == AUTO) ? "AUTO" :
                             (currentMode == MANUAL) ? "MAN" : "STBY";
        oled.setCursor(0, 0);
        oled.print(modeStr);

        oled.setCursor(50, 0);
        oled.printf("B:%u%%", battery);

        oled.setCursor(0, 16);
        oled.printf("L:%u", ldrL);
        oled.setCursor(0, 26);
        oled.printf("R:%u", ldrR);

        int16_t diff = (ldrL + LDR_OFFSET) - ldrR;
        oled.setCursor(0, 38);
        if (abs(diff) < LDR_DEADZONE) oled.print("[=] CENTER");
        else if (diff > 0) oled.print("[>] LEFT");
        else oled.print("[<] RIGHT");

        oled.setCursor(0, 56);
        oled.print("[SEL] Menu");

    } else {
        oled.setCursor(0, 0);
        oled.print("=== MENU ===");

        for (uint8_t i = 0; i < MENU_COUNT && i < 4; i++) {
            oled.setCursor(0, 16 + i * 10);
            if (i == menuPos) oled.print(">");
            oled.print(mainMenu[i].name);
        }
    }
    oled.display();
}

// ============================================
// SLEEP
// ============================================
void sleepMode() {
    playTone(BEEP_SLEEP);
    delay(800);

    oled.clearDisplay();
    oled.setTextSize(2);
    oled.setCursor(10, 20);
    oled.print("SLEEP");
    oled.display();

    digitalWrite(PIN_BUZZER, LOW);
    trackerServo.detach();

    esp_sleep_enable_timer_wakeup(WAKE_INTERVAL_SEC * 1000000ULL);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BTN_SELECT, LOW);

    delay(500);
    esp_deep_sleep_start();
}
