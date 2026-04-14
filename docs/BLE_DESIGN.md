# BLE Integration — Brainstorm & Design

Bluetooth Low Energy (BLE) for the Sun Tracker. Not a gimmick — a showcase feature.

---

## The Goal

**Demo scenario:** Walk up to the tracker, open your phone, and instantly see:
- Real-time LDR readings (live graph)
- Battery voltage & charge status
- Servo position & tracking mode
- System uptime & health
- Historical data (last hour of readings)

All without touching the hardware. Professors love remote monitoring demos.

---

## BLE Use Cases (Ranked by Impact)

### 1. Remote Monitoring Dashboard ⭐⭐⭐⭐⭐
**What:** Phone app reads BLE characteristics showing live sensor data.

**Characteristics:**
| Characteristic | UUID (short) | Type | Description |
|---------------|-------------|------|-------------|
| `ldr_left` | `0x2A01` | Read + Notify | Left LDR value (uint16) |
| `ldr_right` | `0x2A02` | Read + Notify | Right LDR value (uint16) |
| `servo_pos` | `0x2A03` | Read | Servo PWM value (uint8: 0-180) |
| `system_mode` | `0x2A04` | Read + Write | Mode: 0=Auto, 1=Manual, 2=Off |
| `battery_pct` | `0x2A19` | Read + Notify | Battery % (standard BLE Battery Service) |
| `status_flags` | `0x2A05` | Read | Bit flags: bit0=tracking, bit1=night, bit2=error |
| `uptime_sec` | `0x2A06` | Read | Seconds since boot (uint32) |

**Why this wins:**
- Instant visual feedback — open app, see live data
- Uses standard BLE Battery Service UUID (`0x2A19`) — looks professional
- Notify = push updates to phone without polling
- Can use **nRF Connect** (free app) — no custom app needed

---

### 2. Remote Configuration ⭐⭐⭐⭐
**What:** Adjust LDR thresholds and timing from phone — no reflashing needed.

**Characteristics:**
| Characteristic | Type | Description |
|---------------|------|-------------|
| `ldr_threshold` | Read + Write | Min diff to trigger movement (uint16) |
| `ldr_deadzone` | Read + Write | Ignore small differences (uint16) |
| `night_threshold` | Read + Write | Below this = night mode (uint16) |
| `servo_speed` | Read + Write | Servo PWM offset from center (uint8) |

**Why this matters:**
- Calibrate outdoors without laptop
- Tune parameters on the fly
- Shows "production-ready" thinking

---

### 3. Live Data Stream / Graphing ⭐⭐⭐⭐⭐
**What:** Stream LDR values at 5Hz, graph them on phone in real-time.

**How:**
- Use BLE **Notify** on `ldr_left` and `ldr_right`
- Phone app (nRF Connect or custom) plots live graph
- Shows light difference visually — easy to demo tracking behavior

**Demo script:**
1. Open nRF Connect → Connect to "SunTracker"
2. Subscribe to `ldr_left` and `ldr_right` notifications
3. Cover one LDR with hand → watch value drop live
4. Shine phone flashlight → watch value spike
5. Show the tracker responding (servo moves)

**Why this wins:** Real-time data visualization is the #1 "wow" factor in embedded demos.

---

### 4. OTA Firmware Update ⭐⭐⭐
**What:** Update firmware wirelessly via BLE — no USB cable needed.

**How:** Use ESP32's built-in OTA over BLE (requires `ESP32_BLE_OTA` library).

**Why it's cool:** Professors love wireless updates. But it's complex and risky (bricking).

**Verdict:** Phase 5 stretch goal. Only if everything else works perfectly.

---

### 5. Data Logger Export ⭐⭐⭐
**What:** Store last 24 hours of LDR readings in RTC memory, export via BLE.

**How:**
- Every 5 minutes, store `{timestamp, ldr_left, ldr_right}` in RTC RAM
- BLE characteristic exposes log data in chunks
- Phone app downloads and plots daily sun pattern

**Why it's cool:** Shows the full day's sun tracking curve. Great for the presentation.

**Verdict:** Phase 5. Needs RTC memory management.

---

## Recommended: What to Actually Build

**Phase 2.4 (Core BLE):** Implement Use Cases 1 + 3

| Feature | Effort | Impact |
|---------|--------|--------|
| BLE service with 4 characteristics | ~2 hours | High |
| LDR notify updates (5Hz) | ~1 hour | Very High |
| Battery % service (standard UUID) | ~30 min | Medium |
| Mode control via BLE write | ~1 hour | High |
| **Total** | **~4.5 hours** | **Demo-winning** |

**Phase 5 (Stretch):**
- Remote configuration (Use Case 2)
- Data logger export (Use Case 5)
- OTA updates (Use Case 4) — only if ambitious

---

## How the Demo Works

### Setup
1. Upload firmware with BLE enabled
2. ESP32 advertises as **"SunTracker"**
3. Install **nRF Connect** on phone (free, iOS & Android)

### Live Demo
1. Open nRF Connect → Scan → Find "SunTracker"
2. Connect → See service "Sun Tracker Service"
3. Tap arrows to subscribe to notifications
4. LDR values stream live on phone screen
5. Move hand over LDRs → values change in real-time
6. Show servo responding to light changes
7. Write to `system_mode` characteristic → mode changes on device
8. Show battery % reading

### What Judges See
- Wireless, real-time sensor monitoring
- Professional BLE service structure
- Standard UUIDs (Battery Service) — shows knowledge of BLE standards
- Two-way communication (read + write)
- No custom app needed — uses industry-standard tool

---

## Technical Design

### BLE Service UUIDs

```cpp
// Custom service
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"

// Custom characteristics
#define CHAR_LDR_LEFT_UUID     "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // Read + Notify
#define CHAR_LDR_RIGHT_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a9"  // Read + Notify
#define CHAR_SERVO_POS_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26aa"  // Read
#define CHAR_SYSTEM_MODE_UUID  "beb5483e-36e1-4688-b7f5-ea07361b26ab"  // Read + Write
#define CHAR_STATUS_FLAGS_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ac"  // Read

// Standard BLE Battery Service (built into ESP32)
#define BATTERY_SERVICE_UUID   "0000180F-0000-1000-8000-00805F9B34FB"
#define BATTERY_CHAR_UUID      "00002A19-0000-1000-8000-00805F9B34FB"  // Battery Level
```

### Data Flow

```
┌─────────────┐                    ┌─────────────┐
│   ESP32     │   BLE Notify       │   Phone     │
│             │ ──────────────────→│  (nRF       │
│  LDR: 2345  │                    │   Connect)  │
│  LDR: 2312  │   (5Hz stream)     │             │
│  Mode: AUTO │ ◄──────────────────│  (Optional  │
│  Batt: 87%  │   BLE Read/Write   │   write)    │
└─────────────┘                    └─────────────┘
```

### Power Impact

| Scenario | BLE Impact |
|----------|-----------|
| BLE off (deep sleep) | 50 µA |
| BLE advertising (not connected) | ~300 µA |
| BLE connected, idle | ~1-2 mA |
| BLE connected, notifying 5Hz | ~3-5 mA |

**Conclusion:** BLE adds ~3-5 mA when actively connected. Acceptable for daytime operation. In deep sleep, BLE is off — no impact.

---

## Implementation Checklist

### Phase 2.4 — Core BLE
- [ ] Include `BLEDevice.h`, `BLEServer.h`, `BLEUtils.h`, `BLE2902.h`
- [ ] Define service and characteristic UUIDs
- [ ] Create BLE server with callbacks
- [ ] Implement `onWrite` callback for mode control
- [ ] Set up LDR characteristics with notification descriptors
- [ ] Update characteristics in main loop (non-blocking, every 200ms)
- [ ] Add battery % characteristic (standard UUID `0x2A19`)
- [ ] Test with nRF Connect app
- [ ] Verify notifications stream correctly
- [ ] Verify write operations change mode

### Phase 5 — BLE Extensions (Stretch)
- [ ] Remote configuration characteristics
- [ ] Data logger with RTC memory storage
- [ ] BLE data export characteristic
- [ ] OTA firmware update support

---

## Why This Impresses Professors

1. **Standards compliance** — Using standard BLE Battery Service UUID shows you know BLE specifications, not just hacking around
2. **Two-way communication** — Not just reading sensors, but controlling the device remotely
3. **Real-time streaming** — Live data at 5Hz demonstrates BLE Notify capability
4. **No custom app needed** — Using industry-standard nRF Connect shows professionalism
5. **Power-aware design** — BLE off during deep sleep shows system-level thinking
6. **Demo-friendly** — The wireless aspect is the #1 thing judges remember from embedded demos

---

*Recommended library: ESP32's built-in `BLEDevice` — no external dependency needed.*
