# Challenges & Troubleshooting Log

A record of the technical challenges encountered during the development of the Solar Tracker project, along with the root causes and solutions applied. This document is intended for inclusion in the final project report.

---

## 1. Serial Port Permission Denied on Linux

**Symptom:**
```
A fatal error occurred: Could not open /dev/ttyUSB0
([Errno 13] Permission denied: '/dev/ttyUSB0')
```

**Root Cause:**
The current user was not a member of the `dialout` group, which controls access to USB-to-serial devices on Linux.

**Solution:**
```bash
sudo usermod -a -G dialout $USER
```
Logged out and back in for the group change to take effect.

**Lesson Learned:**
Linux requires explicit group membership for hardware access. Always check `groups $USER` when encountering permission errors on development ports.

---

## 2. esptool Crash During Flash Upload

**Symptom:**
```
Uploading stub flasher...
Traceback (most recent call last):
  ...
StopIteration
A fatal error occurred: The chip stopped responding.
```

**Root Cause:**
esptool v5.2.0 has known stability issues with certain ESP32-D0WD-V3 revisions during high-speed (921600 baud) flash writes.

**Solution:**
- Held the BOOT button during upload to force the chip into a stable download mode.
- Reduced upload speed to 115200 baud in Arduino IDE settings.
- Later confirmed esptool v4.5.1 is more stable for this chip revision.

**Lesson Learned:**
Boot mode and baud rate significantly affect flash reliability. Lower speeds are more tolerant of clock drift and power fluctuations.

---

## 3. Servo Spinning Continuously (360° vs 180° Confusion)

**Symptom:**
The servo rotated continuously in one direction regardless of the `write()` value sent. It never stopped at a fixed angle like a standard servo would.

**Root Cause:**
The MG996R purchased was a **continuous rotation** (360°) variant, not a standard positional servo. Continuous rotation servos interpret `write(90)` as "stop," `write(0)` as "full speed one direction," and `write(180)` as "full speed opposite direction." They do not move to specific angles.

**Solution:**
Replaced all angle-based logic with directional speed control:
```cpp
// Standard servo (wrong approach)
servo.write(90);   // would move to 90 degrees

// Continuous rotation (correct approach)
servo.write(90);   // STOP
servo.write(75);   // Slow clockwise
servo.write(105);  // Slow counter-clockwise
```

**Lesson Learned:**
Always verify the exact variant of a component before coding. A "360° servo" is fundamentally different from a "180° servo" in behavior and API usage.

---

## 4. Servo Drifting at Stop Point

**Symptom:**
Even when sending `write(90)`, the servo continued to creep slowly in one direction. Adjusting the code to `write(88)` or `write(92)` did not consistently fix it.

**Root Cause:**
Factory-calibrated servos have an internal potentiometer trim that sets the electrical "center" point. Manufacturing tolerances mean the true stop value may be 88, 90, 92, or even further from 90.

**Solution:**
Added a software trim variable to the code:
```cpp
static constexpr int8_t SERVO_TRIM = 0;
static constexpr uint8_t SERVO_STOP_REAL = 90 + SERVO_TRIM;
```
This allows calibration without recompiling all servo references. The value can be adjusted by ±5 to compensate for the unit's specific offset.

**Lesson Learned:**
No two continuous rotation servos are identical. Always include a calibration mechanism in the firmware for production deployments.

---

## 5. ADC Noise Causing Servo Jitter

**Symptom:**
LDR readings fluctuated by ±150-200 counts even in stable lighting. The servo jittered and oscillated because the code reacted to noise as if it were real light changes.

**Root Cause:**
The ESP32's 12-bit ADC is inherently noisy, especially with WiFi/Bluetooth active and nearby motor switching. Single analog reads are unreliable for precision sensing.

**Solution:**
Implemented a two-stage noise reduction pipeline:
1. **16x Oversampling:** Average 16 consecutive reads (reduces noise by ~4×).
   ```cpp
   uint32_t sum = 0;
   for (uint8_t i = 0; i < 16; i++) {
     sum += analogRead(pin);
   }
   return sum / 16;
   ```
2. **Rolling Average:** Buffer the last 5 oversampled readings and average them (reduces noise by another ~2×).
   ```cpp
   ldrBuf[bufIndex] = rawValue;
   bufIndex = (bufIndex + 1) % 5;
   smoothedValue = average(ldrBuf);
   ```

Combined, this reduced the noise from ±200 to approximately ±20 counts.

**Lesson Learned:**
Raw ADC readings on the ESP32 should never be used directly. Oversampling is mandatory for stable sensor-driven control loops.

---

## 6. Immediate Deep Sleep Indoors

**Symptom:**
The system went to deep sleep immediately after boot, even when placed under room lighting.

**Root Cause:**
The `isNight()` threshold was set to 300 ADC counts. Indoor ambient light typically reads 200-500 on both LDRs, which the code incorrectly classified as "night."

**Solution:**
Temporarily disabled the night detection check for indoor testing with `if (false && isNight())`. For outdoor deployment, the threshold will be raised or made dynamic based on a rolling light baseline.

**Lesson Learned:**
Environmental thresholds must be calibrated for the actual deployment environment. Indoor and outdoor light levels differ by orders of magnitude.

---

## 7. Servo "Lag" with External Power Supply

**Symptom:**
When the servo was powered by an external 5V phone charger, it exhibited erratic twitching, slow response, or failed to move entirely. It worked fine when powered from the ESP32's 5V pin.

**Root Cause:**
The external charger's ground was not connected to the ESP32's ground. Without a common ground reference, the PWM signal from GPIO 12 had no return path, and the servo could not interpret "high" and "low" levels correctly.

**Solution:**
Connected a jumper wire from the external charger's negative terminal to the ESP32 GND pin:
```
[Charger GND] ──> [ESP32 GND] ──> [Servo GND]
```
This established a common ground reference for all components.

**Lesson Learned:**
ALL grounds in a mixed-power system must be connected together. Voltage signals are meaningless without a shared reference point. This is the most common cause of intermittent hardware issues in embedded systems.

---

## 8. LDR Manufacturing Differences

**Symptom:**
Even with both LDRs exposed to identical light, the Serial Monitor showed a consistent difference of 300-500 counts between them. The servo spun continuously toward one side.

**Root Cause:**
LDRs have wide manufacturing tolerances (±20% or more). Two LDRs from the same batch will rarely have identical resistance curves under the same light.

**Solution:**
Added a software offset constant to balance the readings:
```cpp
static constexpr int16_t LDR_OFFSET = 0;  // Tune this value
int16_t diff = (ldrLeft + LDR_OFFSET) - ldrRight;
```
The offset is determined by measuring the difference when both LDRs see equal light and setting `LDR_OFFSET = -RawDiff`.

**Lesson Learned:**
Always include a calibration offset for paired analog sensors. Never assume two components from the same batch will behave identically.

---

## 9. Active Buzzer Cannot Play Melodies

**Symptom:**
The buzzer only produced a single fixed tone (~3.9 kHz). Attempting to change frequency via PWM had no effect.

**Root Cause:**
The purchased buzzer is an **active** buzzer with a built-in oscillator. It accepts only ON/OFF signals. A **passive** buzzer is required to play different notes via PWM frequency control.

**Solution:**
Implemented rhythmic tone patterns (e.g., "to-to-too", "totot") using timed ON/OFF sequences instead of pitch changes. Each system event has a unique rhythmic signature.

**Lesson Learned:**
Component specifications matter. "Active" means built-in tone, "passive" means external control. Always verify the drive method before designing audio feedback.

---

## Summary of Key Lessons

| # | Challenge | Category | Prevention |
|---|-----------|----------|------------|
| 1 | Serial permission | OS/Setup | Check group membership before starting |
| 2 | esptool crash | Toolchain | Use boot mode + lower baud rate |
| 3 | 360° servo behavior | Component knowledge | Read datasheets carefully before coding |
| 4 | Servo stop drift | Calibration | Include software trim in firmware |
| 5 | ADC noise | Signal quality | Always oversample on ESP32 |
| 6 | False night detection | Environment | Calibrate thresholds per environment |
| 7 | Missing common ground | Wiring | Always tie all grounds together |
| 8 | LDR mismatch | Manufacturing | Include software offset for sensor pairs |
| 9 | Active vs passive buzzer | Component knowledge | Verify drive method before purchase |

---
*Documented during development: April 2026*
