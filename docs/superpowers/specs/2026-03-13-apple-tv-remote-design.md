# Apple TV Remote for Flipper Zero — Design Spec

## Overview

A Flipper Zero application that emulates an Apple TV remote using BLE HID and IR, targeting all Apple TV generations. The primary mode is BLE HID (for Apple TV 4th gen and later), with IR as a secondary mode for older models. Built for Momentum custom firmware.

**Goals:**
- Fully functional Apple TV remote from a Flipper Zero
- Clean, intuitive direct-mapping UX (Flipper D-pad = Apple TV navigation)
- App Catalog submission (Momentum and/or official)
- Extensible architecture for future Siri Remote protocol emulation (Phase 3)

**Non-goals (v1):**
- Siri button / voice activation
- Touchpad/swipe gesture emulation
- Accelerometer/gyro data

## Target Devices

| Apple TV Generation | Connection | Supported |
|---|---|---|
| 1st gen (2007) | IR only | Phase 2 |
| 2nd gen (2010) | IR only | Phase 2 |
| 3rd gen (2012) | IR only | Phase 2 |
| 4th gen HD (2015) | IR + BLE | v1 (BLE), Phase 2 (IR) |
| 4K 1st gen (2017) | IR + BLE | v1 (BLE), Phase 2 (IR) |
| 4K 2nd gen (2021) | BLE only | v1 |
| 4K 3rd gen (2022) | BLE only | v1 |

## Architecture

### Operating Modes

1. **BLE HID mode** (primary) — Flipper advertises as a BLE HID peripheral, pairs with Apple TV via standard Bluetooth pairing, sends HID keycodes
2. **IR mode** (Phase 2) — Flipper transmits known Apple IR remote NEC codes via built-in IR blaster

Mode selection is user-configurable and persisted between app launches.

### Input Modes

The app supports configurable input modes, with **Direct Mapping** as the default:

**Direct Mapping (default):**
| Flipper Button | Short Press | Long Press |
|---|---|---|
| D-pad Up | Navigate Up | Volume Up (hold >400ms, repeats ~2.5/sec) |
| D-pad Down | Navigate Down | Volume Down (hold >400ms, repeats ~2.5/sec) |
| D-pad Left | Navigate Left | — |
| D-pad Right | Navigate Right | — |
| OK (center) | Select | — |
| Back | Menu / Back | Exit app (hold >1 second) |

Future input modes (e.g., visual remote layout) can be added as alternatives the user switches to from settings.

### Escape Convention

- **Long-press Back (>1 second):** Exits the app, returns to Flipper menu
- **Short-press Back:** Sends Menu/Back command to Apple TV
- Consistent with standard Flipper app behavior

## BLE HID Implementation

### Pairing Flow

1. User launches app, BLE mode is active (or selected from settings)
2. Flipper starts BLE advertising with device name "FlipperTV Remote"
3. User navigates to Apple TV → Settings → Remotes and Devices → Bluetooth
4. Selects "FlipperTV Remote" from available devices
5. Standard BLE pairing completes (Just Works or PIN confirmation)
6. Flipper screen updates to show "Connected" status
7. D-pad inputs now control the Apple TV

### HID Report Descriptor

Two HID usage pages in one composite report descriptor, separated by Report ID:

- **Report ID 1 — Keyboard (Usage Page 0x07):**
  - Arrow keys (Up, Down, Left, Right) — navigation
  - Enter — select
  - Escape — menu/back

- **Report ID 2 — Consumer Control (Usage Page 0x0C):**
  - Volume Increment (0xE9) — volume up
  - Volume Decrement (0xEA) — volume down
  - Play/Pause (0xCD) — future
  - Mute (0xE2) — future
  - AC Home (0x0223) — future (Home button)

### Connection Management

- **Bond storage:** After initial pairing, the bond is stored so subsequent launches auto-reconnect
- **Reconnection:** If connection drops, app displays "Reconnecting..." and retries automatically with exponential backoff
- **Status indicator:** Always visible on screen — one of: Disconnected / Advertising / Connected
- **Multiple devices:** v1 supports one bonded Apple TV at a time. Future: device selection menu

### Volume Control Behavior

- **Activation:** Hold D-pad Up or Down for >400ms
- **First command:** Fires at the 400ms mark
- **Repeat rate:** Every 400ms (~2.5 increments per second)
- **No acceleration:** Constant rate for predictable control
- **Visual feedback:** Small volume indicator on screen while adjusting
- **Release:** Stops immediately when button is released

## IR Mode (Phase 2)

### Apple Remote IR Protocol

- **Protocol:** NEC (modified Apple variant)
- **Carrier:** 38kHz
- **Known command codes:** Well-documented set of Apple remote IR codes
- **Commands:** Menu, Play/Pause, Up, Down, Left, Right, Select
- **Device ID:** Configurable (Apple TVs can be paired to specific remote IDs to avoid cross-talk)

### Implementation

- Use Flipper's built-in IR subsystem (`infrared` module)
- Ship a bundled `.ir` signal file with all Apple remote commands
- Same direct-mapping UX as BLE mode — user shouldn't notice a difference in how it feels

## UI Design

### Main Screen (Direct Mapping Mode)

```
┌──────────────────────┐
│ FlipperTV Remote     │
│                      │
│   Status: Connected  │
│   Mode: BLE HID     │
│                      │
│   ▲ Nav / ▲▲ Vol+   │
│ ◄   ●   ►           │
│   ▼ Nav / ▼▼ Vol-   │
│                      │
│ [Back: Menu] [OK: ●] │
└──────────────────────┘
```

- Clean, minimal — shows connection status and a hint of the control mapping
- Volume indicator overlays briefly when volume is being adjusted
- Settings accessible via long-press OK from the main screen (enters settings scene, back returns to main)

### Settings Screen

- **Mode:** BLE / IR (Phase 2)
- **Input mode:** Direct Mapping (future: Visual Remote)
- **Device name:** Customizable BLE advertising name
- **Forget pairing:** Clear stored bond, allow re-pairing

### Status Indicators

- **Disconnected:** "Not connected" with blinking BLE icon
- **Advertising:** "Searching..." with animated BLE icon
- **Connected:** Solid connection icon + Apple TV device name if available

## Project Structure

```
flipper-apple-tv-remote/
├── application.fam          # Flipper app manifest
├── README.md
├── SPEC.md                  # → links to this design doc
├── PROJECT_LOG.md
├── docs/
│   └── superpowers/
│       └── specs/
│           └── 2026-03-13-apple-tv-remote-design.md  (this file)
├── src/
│   ├── apple_tv_remote.c    # App entry point, scene management
│   ├── apple_tv_remote.h    # Shared app state/types
│   ├── scenes/
│   │   ├── scene_main.c     # Direct mapping input handler
│   │   ├── scene_settings.c # Settings menu
│   │   └── scene_pairing.c  # BLE pairing flow UI
│   ├── ble/
│   │   ├── ble_hid.c        # BLE HID service setup, report descriptor
│   │   ├── ble_hid.h
│   │   ├── hid_keycodes.h   # Apple TV-relevant HID keycodes
│   │   └── ble_manager.c    # Connection state machine, bonding, reconnect
│   ├── ir/                  # Phase 2
│   │   ├── ir_remote.c
│   │   └── ir_remote.h
│   ├── input/
│   │   ├── input_handler.c  # Button event routing, long-press detection
│   │   └── input_handler.h
│   └── ui/
│       ├── ui_main.c        # Main screen rendering + status indicators
│       └── ui_volume.c      # Volume overlay (has own timing state)
├── assets/
│   └── icons/               # App icon for Flipper menu
└── ir_signals/              # Phase 2: bundled .ir files
    └── apple_remote.ir
```

## Firmware Dependencies

- **Momentum firmware** (latest stable)
- Flipper SDK APIs:
  - `furi_hal_bt` — BLE stack control
  - `bt/bt_service` — BLE HID service
  - `gui` — Scene manager, view dispatcher, UI elements
  - `input` — Button events with press/release/long-press
  - `storage` — Settings persistence
  - `infrared` — IR transmission (Phase 2)

## Testing Strategy

- **Unit testing:** Limited on Flipper — focus on input handler logic (long-press timing, state transitions)
- **Integration testing:** Manual with real Apple TV hardware
  - Pairing flow (first time + reconnect)
  - All button mappings
  - Volume hold behavior (timing, repeat rate)
  - App exit via long-press back
  - Mode switching (when IR is added)
- **Edge cases:**
  - BLE disconnect during use → reconnect behavior
  - Multiple Apple TVs in range → pairing to correct one
  - App backgrounded / Flipper goes to sleep → resume behavior

## Phasing

### v1 — BLE HID Remote
- BLE HID pairing and connection management
- Direct mapping input with core navigation (D-pad, Select, Menu/Back)
- Volume up/down via long-press with slow repeat
- Settings persistence
- App Catalog submission package

### Phase 2 — IR Mode
- Bundled Apple IR remote signals
- IR transmission via Flipper IR blaster
- Mode switching UI (BLE ↔ IR)
- Apple remote device ID configuration

### Phase 3 — Siri Remote Protocol (Exploratory)
- Reverse engineer Siri Remote BLE GATT services
- Explore encrypted pairing bypass or MitM capture
- Touch surface emulation (if feasible)
- Siri button activation

## App Catalog Requirements

- `application.fam` with correct metadata (name, category, version, author)
- App icon (10x10 1-bit)
- README with description, screenshots, usage instructions
- No hardcoded paths or device-specific assumptions
- Clean build against Momentum SDK
- License: GPL-2.0 (matches Momentum firmware license)

## App State & Scene Communication

### App State Struct

```c
typedef enum {
    AppModeBluetoothHid,
    AppModeInfrared,  // Phase 2
} AppMode;

typedef enum {
    BleStateDisconnected,
    BleStateAdvertising,
    BleStateConnected,
} BleState;

typedef struct {
    // Core Flipper app infrastructure
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    // BLE state
    BleState ble_state;
    bool is_bonded;

    // Input state
    bool volume_active;       // true while long-pressing up/down
    FuriTimer* volume_timer;  // fires every 400ms for volume repeat

    // Settings (persisted)
    AppMode mode;
    char device_name[32];
} AppState;
```

### Scene Communication

Scenes communicate via the `SceneManager` custom event system — scenes post events to `ViewDispatcher`, which routes them to the active scene's event handler. The `AppState` struct is passed through the app context pointer, accessible from any scene via `scene_manager_get_context()`.

### Settings Persistence

- **File path:** `APP_DATA_PATH("settings.save")`
- **Format:** FlipperFormat (`.save` extension, human-readable key-value)
- **Fields stored:**
  - `Mode` — `BLE` or `IR`
  - `DeviceName` — BLE advertising name (default: "FlipperTV Remote")
- Bond data is managed by the Flipper BLE stack internally, not by the app

### App Lifecycle & Resource Cleanup

On app exit (long-press Back or system exit):
1. Stop volume repeat timer if active
2. If BLE is advertising or connected: disconnect and restore previous BLE profile via `bt_disconnect()` and `bt_set_profile()` to return to Flipper's default Serial profile
3. Free all allocated views, scene manager, view dispatcher
4. Release GUI record

This ensures the app leaves no BLE resources dangling that would interfere with other Flipper functionality.

### Pairing Resilience

If the Apple TV does not discover the Flipper during initial advertising:
- The main screen shows a "Re-pair" option (short-press OK when disconnected) that restarts BLE advertising
- Advertising timeout: 60 seconds, then returns to Disconnected state with prompt to retry
- If a bond exists but connection fails, the app attempts reconnection 3 times with 2-second intervals before falling back to Disconnected

## Open Questions

1. **App name for catalog:** "FlipperTV Remote"? "Apple TV Remote"? Need something descriptive but not trademark-infringing
2. **Apple TV firmware variations:** Confirm HID keyboard arrow keys work consistently across tvOS versions
3. **Momentum SDK version pinning:** Which minimum Momentum version to target?
