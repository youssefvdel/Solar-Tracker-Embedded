# Setup Guide

Step-by-step instructions to get the Sun Tracker up and running.

---

## 1. Software Setup

### Install Arduino IDE
1. Download from [arduino.cc](https://www.arduino.cc/en/software)
2. Install for your OS (Linux/Windows/macOS)

### Add ESP32 Board Support
1. Open Arduino IDE
2. **File → Preferences**
3. In **Additional Boards Manager URLs**, paste:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Click **OK**
5. **Tools → Board → Boards Manager**
6. Search `esp32` → Install **esp32 by Espressif Systems** (latest version)

### Select Board Settings
1. **Tools → Board → ESP32 Arduino → ESP32 Dev Module**
2. **Tools → Upload Speed → 921600**
3. **Tools → CPU Frequency → 240MHz**
4. **Tools → Flash Frequency → 80MHz**
5. **Tools → Flash Mode → QIO**
6. **Tools → Partition Scheme → Default 4MB**
7. **Tools → Port →** Select your ESP32 serial port (e.g., `/dev/ttyUSB0` or `COM3`)

### Install Libraries
**Method A — Library Manager (recommended):**
1. **Sketch → Include Library → Manage Libraries**
2. Search and install:
   - `ESP32Servo` by John K. Bennett
   - `Adafruit SSD1306` by Adafruit
   - `Adafruit GFX Library` by Adafruit

**Method B — PlatformIO:**
If using PlatformIO, add to `platformio.ini`:
```ini
lib_deps =
    madhephaestus/ESP32Servo@^3.0.5
    adafruit/Adafruit SSD1306@^2.5.10
    adafruit/Adafruit GFX Library@^1.11.10
```

---

## 2. Hardware Assembly

### Step 1 — Power System
1. Set TP4056 boost output to **5.0V**:
   - Connect multimeter to OUT+ and OUT-
   - Adjust trim pot clockwise until reading 5.0V
2. Connect 18650 battery to B+/B- pads (mind polarity!)
3. Connect rocker switch between OUT+ and a 5V rail on breadboard
4. Connect solar panel to IN+/IN- (optional — can charge via USB first)

### Step 2 — ESP32 Placement
1. Place ESP32 on breadboard (straddling the center gap)
2. Connect ESP32 **VIN** to the switched 5V rail
3. Connect ESP32 **GND** to the common GND rail

### Step 3 — LDR Sensors
1. Build two voltage dividers:
   ```
   3.3V ──[LDR]──┬── GPIO 34 (Left)
                 │
              [10kΩ]
                 │
                GND
   ```

2. Connect second LDR to **GPIO 35** (Right):
   ```
   3.3V ──[LDR]──┬── GPIO 35 (Right)
                 │
              [10kΩ]
                 │
                GND
   ```
2. Mount LDRs on opposite sides of the tracker (left and right)
3. Position them so they face the same direction as the solar panel

### Step 4 — Servo
1. Connect **orange wire** → GPIO 12
2. Connect **red wire** → 5V rail (from TP4056 boost output)
3. Connect **brown wire** → GND rail
4. Mount servo on the tracker base
5. Attach the solar panel (or LDR platform) to the servo horn

### Step 5 — OLED Display
1. Connect **VCC** → 3.3V (or 5V)
2. Connect **GND** → GND
3. Connect **SDA** → GPIO 21
4. Connect **SCL** → GPIO 22

### Step 6 — Buzzer
1. Connect **(+)** → GPIO 14
2. Connect **(-)** → GND

### Step 7 — Buttons
Wire each button the same way:
```
GPIO pin ──→ Button leg 1
Button leg 2 ──→ GND
```

| Button | GPIO | Function |
|--------|------|----------|
| BTN 1 | 4 | Mode Toggle |
| BTN 2 | 13 | Calibrate |
| BTN 3 | 15 | Manual Left |
| BTN 4 | 16 | Manual Right |
| BTN 5 | 17 | Reset / Center |
| BTN 6 | 18 | Buzzer Mute |

---

## 3. Upload Firmware

1. Open `Sun_Tracker.ino` in Arduino IDE
2. Connect ESP32 to PC via USB
3. If upload fails, enter flash mode:
   - Hold **BOOT** button
   - Press and release **EN** (reset) button
   - Release **BOOT** button
4. Click **Upload** (→ arrow button)
5. Wait for "Done uploading" message

### Verify Upload
1. Open **Serial Monitor** (magnifying glass icon)
2. Set baud rate to **115200**
3. Press **EN** on ESP32 to reset
4. You should see:
   ```
   Sun Tracker — Starting...
   Servo attached — stopped.
   OLED ready.
   Setup complete.
   LDR Left: 1234 | LDR Right: 1256
   ```

---

## 4. First Power-On (Battery)

1. Ensure rocker switch is **OFF**
2. Verify all connections (see [`WIRING.md`](WIRING.md) checklist)
3. Flip rocker switch **ON**
4. OLED should display splash screen
5. Buzzer should beep briefly
6. Servo should be stopped (center)
7. Check Serial Monitor for LDR readings

---

## 5. Calibration

See [`CALIBRATION.md`](CALIBRATION.md) for LDR threshold calibration.

---

## 6. Troubleshooting

| Problem | Solution |
|---------|----------|
| ESP32 not detected | Check USB cable (some are charge-only), try different port |
| Upload fails | Enter flash mode (BOOT + EN), check board/port settings |
| OLED blank | Check I2C address (try `0x3D`), verify SDA/SCL pins |
| Servo jittering | Check power — use external 5V supply, not ESP32 pin |
| LDR readings stuck | Check voltage divider wiring, verify 10kΩ resistors |
| Buzzer doesn't sound | Verify active buzzer (has built-in oscillator), check polarity |
| Buttons not responding | Check wiring to GND, verify `INPUT_PULLUP` in code |

---

## 7. Next Steps

After successful setup:
1. Calibrate LDR thresholds
2. Implement sun tracking logic
3. Configure button functions
4. Design OLED display layout
5. Test with solar panel outdoors
