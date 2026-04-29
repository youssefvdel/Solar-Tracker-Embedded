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
static constexpr uint8_t PIN_SERVO = 12;
static constexpr uint8_t PIN_LDR_LEFT = 34;
static constexpr uint8_t PIN_LDR_RIGHT = 35;
static constexpr uint8_t PIN_BUZZER = 14;
static constexpr uint8_t PIN_OLED_SDA = 21;
static constexpr uint8_t PIN_OLED_SCL = 22;
static constexpr uint8_t PIN_BTN_LEFT = 4;
static constexpr uint8_t PIN_BTN_SELECT = 18;
static constexpr uint8_t PIN_BTN_RIGHT = 19;
static constexpr uint8_t PIN_LEFT_LIMITER = 15;
static constexpr uint8_t PIN_RIGHT_LIMITER = 13;
static constexpr uint8_t PIN_BATTERY = 36;

// ============================================
// DISPLAY SETTINGS
// ============================================
static constexpr uint8_t SCREEN_W = 128;
static constexpr uint8_t SCREEN_H = 64;
static constexpr uint8_t OLED_ADDR = 0x3C;

// ============================================
// SERVO SETTINGS
// ============================================
static constexpr uint8_t SERVO_STOP = 90;
static constexpr uint8_t SERVO_RIGHT = 0;
static constexpr uint8_t SERVO_LEFT = 180;
static constexpr uint8_t SERVO_WRAP_RIGHT = 45;
static constexpr uint8_t SERVO_WRAP_LEFT = 135;

// ============================================
// LDR SETTINGS
// ============================================
static constexpr uint16_t LDR_SAMPLES = 128;
static constexpr uint16_t LDR_DEADZONE = 200;
static constexpr int16_t LDR_OFFSET = 0;
static constexpr int16_t LDR_HYSTERESIS = 80;
static constexpr uint8_t STUCK_THRESHOLD = 5;

// ============================================
// TIMING INTERVALS (ms)
// ============================================
static constexpr uint32_t TIME_LDR = 200;
static constexpr uint32_t TIME_DISPLAY = 100;
static constexpr uint32_t TIME_BUTTONS = 20;
static constexpr uint32_t DEBOUNCE = 50;

// ============================================
// POWER
// ============================================
static constexpr uint32_t IDLE_TIMEOUT_SEC = 30;
static constexpr uint32_t WAKE_INTERVAL_SEC = 30;

// ============================================
// SERVO PULSE
// ============================================
static constexpr uint32_t SERVO_ON = 15;

// ============================================
// GLOBAL OBJECTS
// ============================================
Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, -1);
Servo trackerServo;

// ============================================
// SYSTEM STATE
// ============================================
enum SysMode
{
    AUTO,
    MANUAL,
    STANDBY
};
enum MenuState
{
    NO_MENU,
    MAIN_MENU
};

SysMode currentMode = AUTO;
MenuState menuState = NO_MENU;

// ============================================
// MENU
// ============================================
void setAutoMode();
void setManualMode();
void enterSleepMode();

struct MenuOption
{
    const char *name;
    void (*action)();
};

const MenuOption mainMenu[] = {
    {"Auto", setAutoMode},
    {"Manual", setManualMode},
    {"Sleep", enterSleepMode}};
static constexpr uint8_t MENU_COUNT = 3;
uint8_t menuPos = 0;
const MenuOption *activeMenu = nullptr;

// ============================================
// SENSORS
// ============================================
uint16_t ldrL = 0, ldrR = 0;
uint16_t bufL[5] = {0}, bufR[5] = {0};
uint8_t bufIdx = 0;
uint8_t battery = 0;

// ============================================
// LIMITERS (debounced, shared between auto + manual)
// ============================================
bool leftLimiter = false;
bool rightLimiter = false;

// ============================================
// TIMING
// ============================================
uint32_t tLDR = 0, tDisplay = 0, tBtn = 0, tBat = 0;
uint32_t tLastMove = 0;
uint32_t tServoStop = 0;
bool servoMoving = false;

// ============================================
// BUTTONS
// ============================================
struct Button
{
    bool pressed = false;
    bool lastState = false;
    uint32_t tChange = 0;
};
Button btn[3];

// ============================================
// BUZZER
// ============================================
static constexpr char BEEP_START[] = "10101101";
static constexpr char BEEP_SCROLL[] = "1";
static constexpr char BEEP_SELECT[] = "101";
static constexpr char BEEP_SLEEP[] = "10101000";

struct ToneGen
{
    const char *pattern;
    uint8_t speed = 100;
    uint8_t index = 0;
    uint32_t tLast = 0;
    bool playing = false;
} beep;

// ============================================
// TRACKING STATE
// ============================================
enum TrackingState { NORMAL, WRAP_LEFT, WRAP_RIGHT };
TrackingState trackingState = NORMAL;
uint8_t stuckCounter = 0;
bool lastDirection = false; // false = left, true = right
bool hasPulsed = false;
int8_t escapeSteps = 0;      // >0 = force right, <0 = force left

// ============================================
// FORWARD DECLARATIONS
// ============================================
void readSensors();
void updateBattery();
void readLimiters();
void checkButtons();
void trackingLogic();
uint16_t readSensor(uint8_t pin);
bool buttonPress(uint8_t pin, uint8_t idx);
void showDisplay();
void sleepMode();
void playTone(const char *pattern);

// ============================================
// SETUP
// ============================================
void setup()
{
    Serial.begin(115200);
    Serial.println("\n=== Solar Tracker Starting ===");

    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);

    pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
    pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
    pinMode(PIN_BTN_SELECT, INPUT_PULLUP);
    pinMode(PIN_LEFT_LIMITER, INPUT_PULLUP);
    pinMode(PIN_RIGHT_LIMITER, INPUT_PULLUP);
    pinMode(PIN_BATTERY, INPUT);

    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR))
    {
        Serial.println("OLED Error!");
        while (1) {
        }
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
void loop()
{
    uint32_t now = millis();

    // Buzzer tone
    if (beep.playing && beep.pattern)
    {
        if (now - beep.tLast >= beep.speed)
        {
            beep.tLast = now;
            char bit = beep.pattern[beep.index];
            if (bit == '\0')
            {
                digitalWrite(PIN_BUZZER, LOW);
                beep.playing = false;
            }
            else
            {
                digitalWrite(PIN_BUZZER, bit == '1' ? HIGH : LOW);
                beep.index++;
            }
        }
    }

    // Stop servo after pulse duration
    if (servoMoving && now >= tServoStop)
    {
        trackerServo.write(SERVO_STOP);
        servoMoving = false;
    }

    // Read debounced limiters (shared between auto and manual)
    readLimiters();

    // Read LDR sensors
    if (now - tLDR >= TIME_LDR)
    {
        tLDR = now;
        readSensors();
        hasPulsed = false; // Allow new pulse

        int16_t diff = (ldrL + LDR_OFFSET) - ldrR;
        Serial.printf("L:%u R:%u D:%d ", ldrL, ldrR, diff);
        Serial.println(abs(diff) < LDR_DEADZONE ? "CENTER" : (diff > 0 ? "LEFT" : "RIGHT"));
    }

    // Battery check
    if (now - tBat >= 1000)
    {
        tBat = now;
        updateBattery();
    }

    // Check buttons
    if (now - tBtn >= TIME_BUTTONS)
    {
        tBtn = now;
        checkButtons();
    }

    // Auto tracking
    if (currentMode == AUTO)
    {
        if (trackingState == NORMAL)
        {
            if (escapeSteps != 0)
            {
                // Forced escape pulses after wrap — move off the limiter bar
                if (!hasPulsed && !servoMoving)
                {
                    trackerServo.write(escapeSteps > 0 ? SERVO_RIGHT : SERVO_LEFT);
                    tServoStop = millis() + SERVO_ON;
                    servoMoving = true;
                    hasPulsed = true;
                    if (escapeSteps > 0)
                        escapeSteps--;
                    else
                        escapeSteps++;
                    Serial.printf("ESCAPE step, remaining=%d\n", escapeSteps);
                }
            }
            else
            {
                trackingLogic();
            }
        }
        else
        {
            // Wrap mode — continuous full-speed rotation
            static uint32_t lastWrapLog = 0;
            if (now - lastWrapLog >= 500)
            {
                lastWrapLog = now;
                Serial.printf("WRAP: state=%s leftL=%d rightL=%d\n",
                              trackingState == WRAP_LEFT ? "LEFT" : "RIGHT",leftLimiter, rightLimiter);
            }

            if (trackingState == WRAP_LEFT)
            {
                if (leftLimiter)
                {
                    trackerServo.write(SERVO_STOP);
                    trackingState = NORMAL;
                    escapeSteps = 5;  // 5 steps to the right to move off the bar
                    stuckCounter = 0;
                    lastDirection = false;
                    Serial.println("WRAP TO LEFT DONE — ESCAPE RIGHT x5");
                }
                else
                {
                    trackerServo.write(SERVO_WRAP_LEFT);
                }
            }
            else if (trackingState == WRAP_RIGHT)
            {
                if (rightLimiter)
                {
                    trackerServo.write(SERVO_STOP);
                    trackingState = NORMAL;
                    escapeSteps = -5;  // 5 steps to the left to move off the bar
                    stuckCounter = 0;
                    lastDirection = true;
                    Serial.println("WRAP TO RIGHT DONE — ESCAPE LEFT x5");
                }
                else
                {
                    trackerServo.write(SERVO_WRAP_RIGHT);
                }
            }
        }
    }

    // Auto sleep removed — was causing resets and timing issues
    // if (currentMode == AUTO && trackingState == NORMAL && escapeSteps == 0 &&
    //     (now - tLastMove) >= (IDLE_TIMEOUT_SEC * 1000UL))
    // {
    //     Serial.println("Idle timeout — sleeping");
    //     sleepMode();
    // }

    // Update display
    if (now - tDisplay >= TIME_DISPLAY)
    {
        tDisplay = now;
        showDisplay();
    }
}

// ============================================
// SENSOR FUNCTIONS
// ============================================
void readSensors()
{
    ldrL = readSensor(PIN_LDR_LEFT);
    ldrR = readSensor(PIN_LDR_RIGHT);
}

uint16_t readSensor(uint8_t pin)
{
    uint32_t total = 0;
    for (uint16_t i = 0; i < LDR_SAMPLES; i++)
    {
        total += analogRead(pin);
    }
    return total / LDR_SAMPLES;
}

// Battery lookup table: voltage (tenths of a volt above 3.2V) → percentage
// Index 0 = 3.2V, Index 10 = 4.2V
static constexpr uint8_t BATT_LUT[] = {0, 5, 15, 30, 45, 55, 65, 75, 85, 95, 100};

uint8_t voltageToPercent(float volts)
{
    if (volts >= 4.2f)
        return 100;
    if (volts <= 3.2f)
        return 0;

    int idx = (int)((volts - 3.2f) * 10.0f);
    idx = constrain(idx, 0, 9);

    float frac = (volts - 3.2f - (idx * 0.1f)) * 10.0f;
    uint8_t low = BATT_LUT[idx];
    uint8_t high = BATT_LUT[idx + 1];

    return low + (uint8_t)((high - low) * frac);
}

void updateBattery()
{
    uint16_t raw = analogRead(PIN_BATTERY);
    float volts = (raw / 4095.0f) * 3.3f * 2.0f;
    battery = voltageToPercent(volts);
    Serial.printf("Bat: %.2fV %u%%\n", volts, battery);
}

// ============================================
// LIMITERS (debounced — called every loop)
// ============================================
void readLimiters()
{
    static uint8_t leftCount = 0;
    static uint8_t rightCount = 0;

    if (digitalRead(PIN_LEFT_LIMITER) == LOW)
    {
        if (++leftCount >= 3)
            leftLimiter = true;
    }
    else
    {
        leftCount = 0;
        leftLimiter = false;
    }

    if (digitalRead(PIN_RIGHT_LIMITER) == LOW)
    {
        if (++rightCount >= 3)
            rightLimiter = true;
    }
    else
    {
        rightCount = 0;
        rightLimiter = false;
    }
}

// ============================================
// TRACKING
// ============================================
void trackingLogic()
{
    if (leftLimiter)
    {
        Serial.println("LIMIT LEFT");
    }
    else if (rightLimiter)
    {
        Serial.println("LIMIT RIGHT");
    }

    int16_t diff = (ldrL + LDR_OFFSET) - ldrR;
    bool newDir = diff > 0;
    Serial.printf("TRACK: diff=%d newDir=%s lastDir=%s\n", diff,
                  newDir ? "RIGHT" : "LEFT",
                  lastDirection ? "RIGHT" : "LEFT");

    // Centered - stop but do NOT reset idle timer
    if (abs(diff) < LDR_DEADZONE)
    {
        trackerServo.write(SERVO_STOP);
        stuckCounter = 0;
        return;
    }

    // STUCK DETECTION: at limit and LDR keeps asking toward it
    if (leftLimiter && !newDir)
    { // trying to move left, but left limit hit → wrap toward RIGHT side
        stuckCounter++;
        Serial.printf("STUCK LEFT %u/%u\n", stuckCounter, STUCK_THRESHOLD);
        if (stuckCounter >= STUCK_THRESHOLD)
        {
            trackingState = WRAP_RIGHT;  // move away from left limit, toward right limit
            servoMoving = false;
            stuckCounter = 0;
            Serial.println("WRAP TO RIGHT START");
            return;
        }
        trackerServo.write(SERVO_STOP);
        lastDirection = false;
        return;
    }
    if (rightLimiter && newDir)
    { // trying to move right, but right limit hit → wrap toward LEFT side
        stuckCounter++;
        Serial.printf("STUCK RIGHT %u/%u\n", stuckCounter, STUCK_THRESHOLD);
        if (stuckCounter >= STUCK_THRESHOLD)
        {
            trackingState = WRAP_LEFT;  // move away from right limit, toward left limit
            servoMoving = false;
            stuckCounter = 0;
            Serial.println("WRAP TO LEFT START");
            return;
        }
        trackerServo.write(SERVO_STOP);
        lastDirection = true;
        return;
    }

    // Not stuck at limit — reset counter
    stuckCounter = 0;

    // Hysteresis check (only when not limited)
    if (newDir != lastDirection && abs(diff) < LDR_HYSTERESIS)
    {
        trackerServo.write(SERVO_STOP);
        return;
    }
    lastDirection = newDir;

    // One non-blocking pulse per LDR cycle
    if (!hasPulsed && !servoMoving)
    {
        hasPulsed = true;
        tLastMove = millis();
        trackerServo.write(lastDirection ? SERVO_RIGHT : SERVO_LEFT);
        tServoStop = millis() + SERVO_ON;
        servoMoving = true;
    }
}

// ============================================
// BUTTONS
// ============================================
bool buttonPress(uint8_t pin, uint8_t idx)
{
    bool raw = digitalRead(pin) == LOW;
    uint32_t now = millis();
    if (raw != btn[idx].lastState)
    {
        btn[idx].tChange = now;
    }
    btn[idx].lastState = raw;

    if (raw && (now - btn[idx].tChange) >= DEBOUNCE)
    {
        if (!btn[idx].pressed)
        {
            btn[idx].pressed = true;
            return true;
        }
    }
    else if (!raw)
    {
        btn[idx].pressed = false;
    }
    return false;
}

void checkButtons()
{
    bool leftEvent = buttonPress(PIN_BTN_LEFT, 0);
    bool rightEvent = buttonPress(PIN_BTN_RIGHT, 1);
    bool selectEvent = buttonPress(PIN_BTN_SELECT, 2);

    // Menu navigation (works in any mode when menu is open)
    if (menuState == MAIN_MENU)
    {
        if (leftEvent && menuPos > 0)
        {
            menuPos--;
            playTone(BEEP_SCROLL);
        }
        if (rightEvent && menuPos < MENU_COUNT - 1)
        {
            menuPos++;
            playTone(BEEP_SCROLL);
        }
        if (selectEvent)
        {
            if (activeMenu && activeMenu[menuPos].action)
            {
                activeMenu[menuPos].action();
            }
            playTone(BEEP_SELECT);
        }
        return;
    }

    // Manual mode servo control — pulsed while holding
    if (currentMode == MANUAL)
    {
        if (!hasPulsed && !servoMoving)
        {
            if (btn[0].pressed && !leftLimiter)
            {
                trackerServo.write(SERVO_RIGHT);
                tServoStop = millis() + SERVO_ON;
                servoMoving = true;
                hasPulsed = true;
            }
            else if (btn[1].pressed && !rightLimiter)
            {
                trackerServo.write(SERVO_LEFT);
                tServoStop = millis() + SERVO_ON;
                servoMoving = true;
                hasPulsed = true;
            }
        }
        if (leftEvent || rightEvent)
            playTone(BEEP_SCROLL);
    }

    // Select button opens menu
    if (selectEvent)
    {
        trackerServo.write(SERVO_STOP);
        servoMoving = false;
        trackingState = NORMAL;
        stuckCounter = 0;
        escapeSteps = 0;
        activeMenu = mainMenu;
        menuState = MAIN_MENU;
        playTone(BEEP_SELECT);
    }
}

void playTone(const char *pattern)
{
    beep.pattern = pattern;
    beep.index = 0;
    beep.tLast = millis();
    beep.playing = true;
}

// ============================================
// MENU ACTIONS
// ============================================
void setAutoMode()
{
    currentMode = AUTO;
    menuState = NO_MENU;
    trackingState = NORMAL;
    stuckCounter = 0;
    escapeSteps = 0;
    trackerServo.write(SERVO_STOP);
    servoMoving = false;
}

void setManualMode()
{
    currentMode = MANUAL;
    menuState = NO_MENU;
    trackingState = NORMAL;
    stuckCounter = 0;
    escapeSteps = 0;
    trackerServo.write(SERVO_STOP);
    servoMoving = false;
}

void enterSleepMode()
{
    menuState = NO_MENU;
    sleepMode();
}

// ============================================
// DISPLAY
// ============================================
void showDisplay()
{
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);

    if (menuState == NO_MENU)
    {
        const char *modeStr = (currentMode == AUTO) ? "AUTO" : (currentMode == MANUAL) ? "MAN" : "STBY";
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
        if (abs(diff) < LDR_DEADZONE)
            oled.print("[=] CENTER");
        else if (diff > 0)
            oled.print("[>] LEFT");
        else
            oled.print("[<] RIGHT");

        if (trackingState != NORMAL)
        {
            oled.setCursor(0, 48);
            oled.print(trackingState == WRAP_LEFT ? "WRAP>" : "<WRAP");
        }

        oled.setCursor(0, 56);
        oled.print("[SEL] Menu");
    }
    else
    {
        oled.setCursor(0, 0);
        oled.print("=== MENU ===");

        for (uint8_t i = 0; i < MENU_COUNT && i < 4; i++)
        {
            oled.setCursor(0, 16 + i * 10);
            if (i == menuPos)
                oled.print(">");
            oled.print(mainMenu[i].name);
        }
    }
    oled.display();
}

// ============================================
// SLEEP
// ============================================
void sleepMode()
{
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
