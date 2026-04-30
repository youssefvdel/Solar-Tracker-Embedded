# Solar Tracker — Mermaid Diagrams

## 1. System Architecture

```mermaid
flowchart TB
    subgraph INPUT["📥 INPUT LAYER"]
        LDR_L["LDR Left\nGPIO 34"]
        LDR_R["LDR Right\nGPIO 35"]
        LIM_L["Limit Left\nGPIO 15"]
        LIM_R["Limit Right\nGPIO 13"]
    end

    subgraph LOGIC["🧠 LOGIC LAYER"]
        READ["Read Sensors\nEvery 200ms"]
        CTR["Centered?\n|diff| < 200"]
        LIM["At Limit?\n+ Stuck 5x"]
        NORMAL["NORMAL\nPulse 15ms"]
        WRAP["WRAP\nContinuous"]
        ESCAPE["ESCAPE\n5 Pulses"]
    end

    subgraph OUTPUT["📤 OUTPUT LAYER"]
        SERVO["Servo\nGPIO 12"]
        OLED["OLED\nI2C"]
        BUZZ["Buzzer\nGPIO 14"]
        SER["Serial\n115200"]
    end

    subgraph UI["🎮 UI LAYER"]
        BTN_L["LEFT\nGPIO 4"]
        BTN_R["RIGHT\nGPIO 19"]
        BTN_S["SELECT\nGPIO 18"]
        MODE_A["AUTO"]
        MODE_M["MANUAL"]
        MODE_S["SLEEP"]
    end

    INPUT --> LOGIC
    UI --> LOGIC
    LOGIC --> OUTPUT

    READ --> CTR
    CTR --"YES"--> NORMAL
    CTR --"NO"--> LIM
    LIM --"NO"--> NORMAL
    LIM --"YES"--> WRAP
    WRAP --> ESCAPE
    ESCAPE --> NORMAL
    NORMAL --> READ
```

---

## 2. Auto Tracking State Machine

```mermaid
stateDiagram-v2
    [*] --> BOOT: Power On
    BOOT --> NORMAL: setup()
    
    NORMAL --> PULSE: |diff| >= 200
    PULSE --> NORMAL: 200ms timer
    
    NORMAL --> STUCK_LEFT: leftLimit + wantLeft x5
    NORMAL --> STUCK_RIGHT: rightLimit + wantRight x5
    
    STUCK_LEFT --> WRAP_RIGHT
    STUCK_RIGHT --> WRAP_LEFT
    
    WRAP_LEFT --> ESCAPE_LEFT: hit leftLimit
    WRAP_RIGHT --> ESCAPE_RIGHT: hit rightLimit
    
    ESCAPE_LEFT --> NORMAL: 5 pulses done
    ESCAPE_RIGHT --> NORMAL: 5 pulses done
    
    NORMAL --> MANUAL: Menu → Manual
    MANUAL --> NORMAL: Menu → Auto
    
    NORMAL --> SLEEP: Menu → Sleep
    MANUAL --> SLEEP: Menu → Sleep
    SLEEP --> BOOT: Wake (timer/button)
```

---

## 3. Main Loop Flow

```mermaid
flowchart TD
    START([Start]) --> LOOP{Main Loop}
    LOOP --> BUZZ[Update Buzzer]
    LOOP --> SERVO_STOP[Check Servo Timer]
    LOOP --> LIMIT[Read Limiters]
    LOOP --> LDR[Read LDRs]
    LOOP --> BTN[Check Buttons]
    LOOP --> AUTO[Auto Tracking?]
    LOOP --> DISP[Update Display]
    
    AUTO -->|NO| LOOP
    AUTO -->|YES| TRACK{trackingState}
    
    TRACK -->|NORMAL| ESC{escapeSteps?}
    ESC -->|>0| DO_ESC[Escape Pulse]
    ESC -->|0| LOGIC[trackingLogic]
    
    LOGIC --> CTR{|diff| < 200?}
    CTR -->|YES| STOP[Stop Servo]
    CTR -->|NO| LIM_CHK{At Limit?}
    
    LIM_CHK -->|NO| HYST{Dir Changed?}
    LIM_CHK -->|YES| STUCK{Stuck 5x?}
    
    STUCK -->|NO| STOP2[Stop + Block Dir]
    STUCK -->|YES| SET_WRAP[Set WRAP Mode]
    
    HYST -->|YES| STOP3[Stop]
    HYST -->|NO| DO_PULSE[Pulse Toward Light]
    
    TRACK -->|WRAP_LEFT| WL{leftLimit?}
    WL -->|NO| ROT_L[Rotate Left]
    WL -->|YES| SET_ESC_L[Set Escape Right]
    
    TRACK -->|WRAP_RIGHT| WR{rightLimit?}
    WR -->|NO| ROT_R[Rotate Right]
    WR -->|YES| SET_ESC_R[Set Escape Left]
    
    DO_ESC --> LOOP
    DO_PULSE --> LOOP
    STOP --> LOOP
    STOP2 --> LOOP
    STOP3 --> LOOP
    SET_WRAP --> LOOP
    ROT_L --> LOOP
    ROT_R --> LOOP
    SET_ESC_L --> LOOP
    SET_ESC_R --> LOOP
```

---

## 4. Component Wiring Diagram

```mermaid
flowchart LR
    subgraph ESP32["ESP32 Dev Board"]
        G4[GPIO 4]
        G12[GPIO 12]
        G13[GPIO 13]
        G14[GPIO 14]
        G15[GPIO 15]
        G18[GPIO 18]
        G19[GPIO 19]
        G21[GPIO 21]
        G22[GPIO 22]
        G34[GPIO 34]
        G35[GPIO 35]
        G36[GPIO 36]
    end

    G4 --> BTN_L[Button LEFT]
    G19 --> BTN_R[Button RIGHT]
    G18 --> BTN_S[Button SELECT]
    
    G12 --> SERVO[Servo Signal]
    G13 --> LIM_R[Limit Right]
    G15 --> LIM_L[Limit Left]
    
    G14 --> BUZZ[Buzzer]
    G21 --> OLED_SDA[OLED SDA]
    G22 --> OLED_SCL[OLED SCL]
    
    G34 --> LDR_L[LDR Left]
    G35 --> LDR_R[LDR Right]
    G36 --> BATT[Battery Sense]
```

---

## Styling Reference (for Figma)

| Element | Fill | Stroke | Text |
|---------|------|--------|------|
| Input Layer | `#1e3a5f` | `#4a9eed` | `#4a9eed` |
| Logic Layer | `#2d1b69` | `#8b5cf6` | `#a78bfa` |
| Output Layer | `#1a4d2e` | `#22c55e` | `#22c55e` |
| UI Layer | `#5c3d1a` | `#f59e0b` | `#f59e0b` |
| Decision Diamond | `#5c3d1a` | `#f59e0b` | `#f59e0b` |
| Normal State | `#1a4d2e` | `#22c55e` | `#22c55e` |
| Wrap State | `#5c1a1a` | `#ef4444` | `#ef4444` |
| Escape State | `#5c3d1a` | `#f59e0b` | `#f59e0b` |
| YES arrow | — | `#22c55e` | `#22c55e` |
| NO arrow | — | `#ef4444` | `#ef4444` |
| Background | `#1e1e2e` | — | `#e5e5e5` |

Font: Inter or Arial, 12-16px for labels, 20-28px for titles
