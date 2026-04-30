# Implementation Steps — Solar Tracker Embedded System  
**Version 1.0.0**

---

## 8.1 Overview

This chapter documents the complete assembly and construction process of the solar tracker embedded system. The implementation proceeds through four sequential phases: hardware procurement and mechanical assembly, electrical wiring and voltage divider construction, software environment configuration and firmware deployment, and finally calibration and field testing. Each phase is presented in the order it was executed, with design rationale provided for key decisions.

---

## 8.2 Hardware Procurement

The following components were sourced prior to assembly. All parts are commercially available and were selected to balance cost against functional requirements.

**Microcontroller.** An ESP32 Dev Module (DOIT/NODEMCU-32S) was chosen for its integrated Wi-Fi and Bluetooth Classic radios, 12-bit SAR ADC, and two 240 MHz cores. The dual-core architecture permits future firmware expansion without hardware revision. The 4 MB flash partition accommodates the compiled firmware (1.13 MB) with room for over-the-air update support.

**Actuator.** A 360° continuous-rotation servo motor (FS90R-compatible) drives the solar panel assembly. Unlike standard positional servos, a continuous servo accepts speed and direction commands via pulse-width modulation rather than absolute angle commands. The selected model operates across a 4.8–6.0 V range and draws approximately 750 mA at stall — a figure that informed the power supply design.

**Sensors.** Two GL5528 cadmium-sulphide photoresistors (LDRs) provide differential light intensity measurement. Their spectral response peaks near 540 nm with sensitivity extending into the near-infrared, making them suitable for sunlight tracking. Each LDR is paired with a 10 kΩ fixed resistor in a voltage divider configuration, producing a 0–3.3 V signal proportional to incident illuminance.

Position feedback is obtained not from an encoder but from two copper-bar limit switches. A strip of copper fixed to the rotating assembly contacts stationary copper pads at each end of the permitted travel arc. The mechanical simplicity eliminates the cost and complexity of rotary encoders while providing unambiguous end-of-travel detection.

**Display.** An SSD1306-driven 128×64 pixel OLED module communicates over I²C (address 0x3C). The monochrome display draws approximately 20 mA at full brightness and provides four lines of status information: operating mode, battery percentage, LDR readings, and computed tracking direction.

**Power.** A single protected 18650 lithium-ion cell (nominal 3.7 V, full charge 4.1 V) supplies the system. A voltage divider composed of two 10 kΩ resistors scales the battery voltage by a factor of two for measurement at GPIO 36. A 1000 µF electrolytic capacitor is placed across the servo power rail to absorb the inrush current during motor start-up, preventing the ESP32 from experiencing a brownout reset.

**Additional components** include three tactile push-buttons for local user input, a passive piezo buzzer for audible feedback, a breadboard for prototyping, and assorted jumper wires.

A complete bill of materials is provided in Appendix A.

---

## 8.3 Mechanical Assembly

### 8.3.1 Base Platform

The assembly begins with a rigid base platform constructed from 5 mm plywood. The platform dimensions (200 mm × 150 mm) provide sufficient area for the ESP32, breadboard, battery holder, and servo mounting bracket while maintaining portability. The platform was sanded and sealed with a clear acrylic coating for moisture resistance.

### 8.3.2 Servo Mounting

The servo motor is fixed to the base platform using two M2 machine screws through pre-drilled holes. The servo horn is oriented at the midpoint of its rotation before tightening to ensure symmetric travel in both directions. A lightweight aluminium bracket, bent to a 90° angle, attaches to the servo horn and provides a mounting surface for the solar panel assembly.

### 8.3.3 LDR Placement

Two LDRs are mounted on the front face of the solar panel bracket, positioned at the left and right edges respectively. Each LDR is angled outward at approximately 45° from the panel normal. This orientation creates a differential sensitivity pattern: when the panel faces the light source directly, both LDRs receive equal illumination; when the panel is misaligned, one LDR receives more light than the other, producing a signed error signal proportional to the angular offset.

The LDRs are enclosed in short sections of black heat-shrink tubing, leaving only the sensor face exposed. This shield prevents light from entering the sensor body at oblique angles, improving the directionality of the measurement.

### 8.3.4 Limit Switch Assembly

Two stationary copper contact pads are fixed to the base platform at positions corresponding to ±60° of servo rotation. A 40 mm × 5 mm copper strip is soldered to the rotating bracket such that it bridges the gap between the servo shaft and the stationary pad at each extreme of travel.

The stationary pads are wired to GPIO 15 (left limit) and GPIO 13 (right limit) with the ESP32 internal pull-up resistor enabled. The copper bar is connected directly to ground. When the rotating assembly reaches a limit, the copper bar contacts the stationary pad, pulling the corresponding GPIO line LOW. This configuration was chosen over normally-closed switches because a broken or disconnected wire results in a permanently HIGH reading — interpreted as "limit not touched" — which fails safe by preventing movement rather than allowing uncontrolled rotation.

Early prototypes exhibited false triggers due to contact bounce as the copper bar skimmed the pad. This was mitigated in firmware through a software debounce routine that samples the GPIO five times over 5 ms and requires a minimum of four LOW readings before registering a valid contact.

### 8.3.5 Enclosure

A clear acrylic enclosure houses the electronics. Ventilation holes (4 mm diameter) are drilled on the underside to prevent condensation. Cable entry points are sealed with rubber grommets. The OLED display is mounted flush against the enclosure wall with a cut-out window for visibility.

---

## 8.4 Electrical Wiring

### 8.4.1 Wiring Table

All connections were made using 22 AWG solid-core jumper wire on a solderless breadboard. The following table documents every GPIO assignment.

| GPIO | Signal          | Connection                                        |
|------|-----------------|---------------------------------------------------|
| 4    | Button Left     | Tactile switch to GND                              |
| 19   | Button Right    | Tactile switch to GND                              |
| 18   | Button Select   | Tactile switch to GND                              |
| 34   | LDR Left        | Junction of 10 kΩ (to 3.3V) and LDR (to GND)      |
| 35   | LDR Right       | Junction of 10 kΩ (to 3.3V) and LDR (to GND)      |
| 36   | Battery Sense   | Junction of 10 kΩ (to BAT+) and 10 kΩ (to GND)    |
| 15   | Limit Left      | Stationary copper pad                              |
| 13   | Limit Right     | Stationary copper pad                              |
| 12   | Servo Signal    | Servo control wire (orange)                        |
| 14   | Buzzer          | Piezo element (passive)                            |
| 21   | OLED SDA        | I²C serial data                                    |
| 22   | OLED SCL        | I²C serial clock                                   |
| —    | 5V Rail         | Servo VCC                                          |
| —    | 3.3V Rail        | OLED VCC, LDR voltage dividers                     |
| —    | GND Rail         | Common ground for all components                   |

### 8.4.2 Voltage Divider Networks

Three identical voltage divider circuits are employed.

**LDR dividers.** Each LDR divider consists of a 10 kΩ resistor connected from the 3.3 V rail to the ADC input (GPIO 34 or 35), and the LDR connected from the ADC input to ground. Under bright illumination the LDR resistance drops to approximately 1 kΩ, pulling the ADC input toward ground (low voltage). Under darkness the LDR resistance rises above 100 kΩ, pulling the ADC input toward 3.3 V (high voltage). The firmware inverts this relationship — higher ADC readings correspond to brighter light — to produce an intuitive signal where the brighter side yields the larger numerical value.

**Battery divider.** The battery voltage divider uses two 10 kΩ resistors in series between the battery positive terminal and ground. The midpoint is connected to GPIO 36. With a 4.1 V battery at full charge, the ADC pin receives 2.05 V — safely within the 0–3.6 V measurement range configured by the 11 dB attenuation setting. The firmware multiplies the measured voltage by the divider ratio (2.30, calibrated) to recover the actual battery voltage.

### 8.4.3 Power Distribution

The servo motor is powered from the 5 V rail (VIN pin) rather than the 3.3 V regulator output. Continuous servos draw significant current during stall conditions; connecting to the 3.3 V rail would exceed the regulator's 500 mA rating and cause the ESP32 to reset. A 1000 µF electrolytic capacitor is soldered across the servo power terminals to smooth the current demand during motor start-up. The OLED, LDR dividers, and other low-current peripherals are supplied from the 3.3 V rail.

---

## 8.5 Software Environment

### 8.5.1 Toolchain Installation

The firmware is developed and deployed exclusively through the Arduino CLI (`arduino-cli`), avoiding the graphical IDE in favour of reproducible command-line workflows.

**Step 1 — Install Arduino CLI.** On Fedora 43, the CLI is available through the distribution package manager:

```bash
sudo dnf install arduino-cli
arduino-cli version
```

**Step 2 — Add ESP32 board support.** The Espressif board definition package is added to the CLI configuration:

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

This installs the ESP32 toolchain, esptool flash utility, and the Arduino-ESP32 core (version 3.3.8).

**Step 3 — Install peripheral libraries.** Three external libraries are required beyond those bundled with the ESP32 core:

```bash
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "Adafruit SSD1306"
arduino-cli lib install "ESP32Servo"
```

The Adafruit GFX and SSD1306 libraries provide the graphics primitives and I²C driver for the OLED display. The ESP32Servo library implements hardware PWM servo control using the ESP32's LEDC peripheral, which supports up to 16 independent PWM channels — sufficient for a single servo with room for expansion.

The `BluetoothSerial.h` and `Wire.h` headers are included in the ESP32 core distribution and require no separate installation.

### 8.5.2 Board Verification

With the ESP32 connected via USB, the CLI detects the device:

```bash
arduino-cli board list
```

The expected output identifies the device at `/dev/ttyUSB0`. If multiple serial devices are present, the ESP32 can be distinguished by its USB vendor ID after running `lsusb`.

### 8.5.3 Compilation and Upload

The firmware is compiled for the `esp32:esp32:esp32` fully-qualified board name:

```bash
cd Solar-Tracker-Embedded
arduino-cli compile --fqbn esp32:esp32:esp32 .
```

A successful compilation reports approximately 1,131,000 bytes of program storage (86% of available flash) and 43,980 bytes of dynamic memory (13% of available RAM). The Bluetooth stack accounts for the majority of flash usage; the core control logic occupies less than 40 kB.

Upload is performed over the same USB connection:

```bash
arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 .
```

If the upload fails with a port-busy error, the serial monitor process from a previous session must be terminated before retrying:

```bash
fuser -k /dev/ttyUSB0
```

A hard reset (unplug USB, wait five seconds, reconnect) resolves persistent esptool communication failures caused by the CP2102 USB-to-serial bridge entering an indeterminate state after multiple rapid upload cycles.

---

## 8.6 Calibration

### 8.6.1 Battery Voltage Calibration

The battery voltage measurement chain involves a voltage divider, the ESP32 ADC, and a firmware reference voltage of 3.3 V. Manufacturing tolerances in the 10 kΩ resistors (±5%) and the ESP32's internal voltage reference (±6%) combine to produce a systematic offset between the measured and actual battery voltage. The `BATT_DIVIDER` constant in the firmware absorbs this offset.

The calibration procedure is as follows:

1. Measure the actual battery terminal voltage with a digital multimeter. Record this value as V_actual.
2. Connect the ESP32 to a computer and open the serial monitor at 115200 baud.
3. Observe the status line, which prints the firmware-computed voltage in the format `B:percentage(raw,voltageV)`.
4. Record the firmware-reported voltage as V_reported.
5. Compute the corrected divider ratio:

   ```
   BATT_DIVIDER_new = BATT_DIVIDER_current × (V_actual / V_reported)
   ```

6. Update `BATT_DIVIDER` in the source code, recompile, and upload.
7. Verify that the serial monitor now reports a voltage within ±0.05 V of the multimeter reading.

For the prototype described in this report, the calibrated divider value converged to 2.30 after two iterations, compared to the theoretical value of 2.00. The discrepancy arises primarily from the ESP32 ADC's deviation from the nominal 3.3 V reference.

The battery percentage is derived from a non-linear lookup table (LUT) that maps the linear ADC voltage fraction to the characteristic discharge curve of a lithium-ion cell. Eleven entries span the range from 3.35 V (0%, safe cutoff) to 4.10 V (100%, full charge). The non-linear mapping was empirically tuned using discharge data to ensure that the 50% reading corresponds to the nominal 3.85 V plateau of a Li-ion cell.

### 8.6.2 LDR Threshold

The `LDR_THRESH` constant (default 100 ADC units) defines the minimum differential between the left and right LDR readings required to trigger a tracking movement. A value that is too low causes the tracker to oscillate around the equilibrium point; a value that is too high causes sluggish response to changing light conditions.

Calibration is performed by illuminating the panel with evenly distributed light and observing the serial monitor. The `d:` field (difference) should be near zero. Shading one LDR should produce a difference comfortably exceeding the threshold. The default value of 100 was found to provide stable tracking under both direct sunlight and overcast conditions with the GL5528 LDRs and 10 kΩ divider resistors.

### 8.6.3 Servo Pulse Timing

Two constants govern the servo stepping behaviour:

- `PULSE_MS`: the duration (in milliseconds) for which the servo is driven at full speed before being stopped. Longer pulses produce larger angular steps.
- `PULSE_WAIT_MS`: the idle interval between consecutive pulses. A value of zero eliminates the pause, maximising tracking speed.

For the prototype, `PULSE_MS = 20` and `PULSE_WAIT_MS = 0` were selected after testing values from 10 ms to 100 ms. The 20 ms pulse provides a visible but controlled angular step, and the absence of a wait interval ensures that the servo responds promptly to changes in light direction without generating excessive heat.

### 8.6.4 Limit Switch Verification

With the firmware running and the serial monitor open, manually rotating the assembly until the copper bar contacts the left stationary pad should cause the `LL` field to change from `-` to `T` (touched). The same applies to the right side for the `RL` field.

If a limit is never detected, the most common cause is incorrect grounding: the copper bar must connect to GND, not to VCC. With the GPIO configured in INPUT_PULLUP mode, the pin is internally pulled HIGH. Contact must pull it LOW to register. The firmware debounce routine (five samples over 5 ms, minimum four LOW) prevents false positives from contact bounce.

---

## 8.7 Operational Testing

The following test protocol was executed to verify each operating mode of the firmware.

### 8.7.1 Startup Sequence

On application of power, the ESP32 bootloader initialises and the `setup()` routine executes. The expected behaviour is:

1. An ascending three-note tone (C5–E5–G5) plays on the piezo buzzer.
2. The OLED initialises and displays the current mode (`AUTO`), battery percentage, LDR readings, and computed tracking direction.
3. With the LDRs evenly illuminated, the direction field reads `CENTER`.

### 8.7.2 Automatic Tracking Mode

With the system in AUTO mode, shading the left LDR causes the servo to pulse RIGHT (toward the brighter right LDR). Shading the right LDR causes the opposite response. When both LDRs receive equal light, the servo remains stationary and the display reads `CENTER`.

After 30 seconds of continuous equal illumination (no tracking movement), the firmware transitions to SLEEP mode. This idle timeout is controlled by the `IDLE_MS` constant and was verified with a stopwatch.

### 8.7.3 Limit and Wrap Behaviour

When the servo approaches a limit switch and the firmware determines that the next pulse would drive the assembly further into the limit, the pulse is blocked and a stuck counter increments. The serial monitor prints `BLOCKED stuckCount=N` on each blocked iteration. After five consecutive blocked attempts, the firmware enters WRAP mode:

1. The servo rotates continuously at full speed in the opposite direction.
2. It continues until the opposing limit switch is contacted.
3. Five escape pulses are then issued in the original direction to back the assembly away from the newly reached limit.
4. Normal AUTO tracking resumes.

The entire wrap sequence completes in approximately five seconds on the prototype hardware with the 10-second safety timeout never triggered under normal operation.

### 8.7.4 Manual Control Mode

MANUAL mode is entered through the on-device menu. While in this mode, holding the LEFT button causes continuous leftward servo pulses; holding the RIGHT button drives the servo rightward. The limiter protection remains active: if the assembly is already contacting a limit switch, pulses in the direction of that limiter are blocked, but pulses away from it are permitted. This allows the operator to recover from a limit condition without switching back to AUTO mode.

### 8.7.5 Sleep and Wake

SLEEP mode may be entered automatically (after the AUTO idle timeout) or manually (through the menu). On entry, a descending tone plays and the OLED displays the word "SLEEP". The ESP32 enters light sleep, waking every 500 ms to poll the buttons and — at a configurable interval (`SLEEP_CHECK_MS`, default 30 seconds) — the LDRs.

Pressing any button wakes the system immediately (within the 500 ms polling window) and restores AUTO mode with an ascending wake-up tone. If a significant LDR difference is detected during the periodic check, the system also wakes to AUTO mode. Between checks, the light sleep state reduces current consumption from approximately 80 mA to under 5 mA, extending battery life.

### 8.7.6 Menu System

Pressing the SELECT button opens an on-device menu that cycles through the three user-facing modes (AUTO, MANUAL, SLEEP). The LEFT and RIGHT buttons navigate the options; SELECT confirms. Each button press produces an audible click from the buzzer. The menu opens with an ascending chime and blocks normal firmware operation until dismissed, ensuring that accidental button presses during tracking do not interfere with servo control.

---

## 8.8 Bluetooth Remote Control

The ESP32's integrated Bluetooth Classic radio provides wireless remote control without additional hardware. The device advertises as `SolarTracker` using the Serial Port Profile (SPP).

### 8.8.1 Pairing

From an Android device, the "Serial Bluetooth Terminal" application is used to scan for and connect to the tracker. No PIN is required. The connection is maintained as long as the application is open and within range (approximately 10 metres in open air).

### 8.8.2 Command Set

Commands are ASCII strings terminated by a newline character. The firmware matches commands case-insensitively against the following vocabulary:

| Command         | Effect                              |
|-----------------|-------------------------------------|
| `A` or `AUTO`   | Switch to AUTO tracking mode        |
| `M` or `MANUAL` | Switch to MANUAL control mode       |
| `S` or `SLEEP`  | Switch to SLEEP power-saving mode   |
| `L` or `LEFT`   | Issue a single leftward servo pulse |
| `R` or `RIGHT`  | Issue a single rightward servo pulse|
| `?` or `STATUS` | Request immediate telemetry reply   |

Manual pulse commands (`L`/`R`) are only honoured when the tracker is in MANUAL mode; in other modes they return an error string.

### 8.8.3 Telemetry

An automatic telemetry string is transmitted once per second. The format is:

```
MODE:AUTO B:85 L:2045 R:1890 D:LEFT
```

The fields represent the current operating mode, battery percentage, left and right LDR readings (raw ADC units), and computed tracking direction (LEFT, RIGHT, or CENTER). This stream enables remote monitoring without requiring a physical connection to the serial port.

---

## 8.9 Deployment Considerations

The assembled tracker is intended for outdoor deployment. Several measures were taken to ensure reliable long-term operation.

The acrylic enclosure provides protection against rain and dust ingress while allowing the OLED display to remain visible. Ventilation holes prevent internal condensation, which could short-circuit exposed contacts on the breadboard.

The copper limit switch contacts were burnished with fine-grit sandpaper to remove oxidation and ensure low-resistance electrical contact. Periodic cleaning is recommended if the tracker is deployed in humid or coastal environments.

The 18650 cell is housed in a removable holder, facilitating field replacement without tools. A protection circuit module (PCM) integrated into the cell prevents over-discharge below 2.5 V, over-charge above 4.25 V, and short-circuit events — all of which are safety hazards in lithium-ion chemistry.

The 1000 µF capacitor across the servo power terminals was found to be essential. Without it, the inrush current during servo start-up caused the ESP32 supply voltage to sag below the brownout threshold, triggering an unintended reset. This failure mode is not immediately obvious during bench testing with a regulated USB supply but becomes apparent when operating from battery power, where the source impedance is higher.

---

## 8.10 Summary

The solar tracker embedded system was constructed in four phases over a period of approximately three weeks. Mechanical assembly established the physical platform and sensor geometry. Electrical wiring implemented the power distribution, sensor interfaces, and actuator control. Software configuration provided the development toolchain and firmware deployment pipeline. Calibration and testing validated each operating mode against its design specification.

The final system achieves closed-loop solar tracking with automatic limit detection and unwrap behaviour, manual override through both physical buttons and Bluetooth, a power-saving sleep mode with conditional wake, and real-time status display on the integrated OLED. The total bill of materials cost is under 200 EGP, making the design accessible for educational and small-scale deployment contexts.
