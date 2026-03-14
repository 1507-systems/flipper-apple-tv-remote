# Apple TV Remote v1 — Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Flipper Zero BLE HID remote that controls Apple TV via direct D-pad mapping with volume long-press.

**Architecture:** Single Flipper FAP using ViewDispatcher with two views (remote + settings). BLE HID profile sends keyboard arrow keys for navigation and consumer control codes for volume. Settings persisted via FlipperFormat.

**Tech Stack:** C (Flipper Zero SDK), Momentum firmware, BLE HID (HOGP), FlipperFormat for persistence

**Spec:** `docs/superpowers/specs/2026-03-13-apple-tv-remote-design.md`

**Build note:** Individual task commits may not compile in isolation. The project is expected to compile after all of Chunk 2 is complete (Task 6). Earlier commits are logical checkpoints, not buildable milestones.

**Deferred from spec (v1.1):**
- Device name edit UI (Flipper text input widget is complex — not worth the complexity for v1; the stored `device_name` field is ready to wire up when we add it)
- BLE advertising timeout (60s) — advertising runs until connection or app exit
- Reconnection retry with backoff — the BLE stack handles basic reconnection; explicit retry logic deferred
- "Re-pair" action on short-press OK when disconnected — for v1, user uses Settings > Forget Pairing to re-pair

---

## File Structure

```
flipper-apple-tv-remote/
├── application.fam              # App manifest — metadata, category, icon ref
├── apple_tv_remote.c            # Entry point — alloc, BLE setup, cleanup
├── apple_tv_remote.h            # AppState struct, enums, view IDs, shared constants
├── views/
│   ├── remote_view.c            # Remote view — input handling, volume timer, draw callback
│   ├── remote_view.h            # RemoteView alloc/free/get_view, set_connected callback
│   ├── settings_view.c          # Settings submenu — device name, forget pairing
│   └── settings_view.h          # SettingsView alloc/free/get_view
├── settings/
│   ├── settings_storage.c       # FlipperFormat save/load for app config
│   └── settings_storage.h       # AppSettings struct, save/load functions
├── assets/
│   └── icons/
│       └── remote_10px.png      # 10x10 1-bit app icon
├── SPEC.md
├── PROJECT_LOG.md
└── docs/                        # (already exists — spec lives here)
```

**Why this layout:** Flipper FAPs conventionally keep sources flat or in `views/`. The spec's deep `src/ble/`, `src/input/`, `src/ui/` tree is over-modularized for what amounts to ~500 lines of C. The BLE setup/teardown lives in the entry point (following the `bt_hid` app pattern). Input handling and drawing live in the remote view (they're tightly coupled — separating them adds indirection without benefit).

---

## Chunk 1: Project Scaffold & App Entry Point

### Task 1: Application manifest and shared header

**Files:**
- Create: `application.fam`
- Create: `apple_tv_remote.h`

- [ ] **Step 1: Create `application.fam`**

```python
App(
    appid="apple_tv_remote",
    name="Apple TV Remote",
    apptype=FlipperAppType.EXTERNAL,
    entry_point="apple_tv_remote_app",
    stack_size=2 * 1024,
    fap_description="Control Apple TV via BLE HID",
    fap_category="Bluetooth",
    fap_icon="assets/icons/remote_10px.png",
    fap_version=(1, 0),
    fap_author="pickleresistor",
)
```

- [ ] **Step 2: Create `apple_tv_remote.h`**

This is the shared state header — every other file includes it.

```c
#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <bt/bt_service/bt.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <furi_hal_bt_hid.h>
#include <furi_hal_usb_hid.h>

#define TAG "AppleTVRemote"
#define BT_KEYS_STORAGE_NAME ".bt_hid.keys"

// How long a button must be held before volume mode activates (ms)
#define VOLUME_HOLD_THRESHOLD_MS 400
// Interval between volume repeat commands (ms) — ~2.5/sec
#define VOLUME_REPEAT_INTERVAL_MS 400

typedef enum {
    ViewIdRemote,
    ViewIdSettings,
} ViewId;

typedef enum {
    BleStateDisconnected,
    BleStateAdvertising,
    BleStateConnected,
} BleState;

typedef struct {
    char device_name[32];
} AppSettings;

typedef struct {
    Gui* gui;
    Bt* bt;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;

    // Views (owned)
    View* remote_view;
    Submenu* settings_submenu;

    // BLE state
    BleState ble_state;

    // Settings (persisted)
    AppSettings settings;
} App;
```

- [ ] **Step 3: Commit**

```bash
git add application.fam apple_tv_remote.h
git commit -m "feat: add app manifest and shared header"
```

---

### Task 2: App entry point with BLE lifecycle

**Files:**
- Create: `apple_tv_remote.c`
- Reference: `apple_tv_remote.h`

- [ ] **Step 1: Create `apple_tv_remote.c` with alloc/free/main**

```c
#include "apple_tv_remote.h"
#include "views/remote_view.h"
#include "views/settings_view.h"
#include "settings/settings_storage.h"

// Navigation callbacks — ViewDispatcher calls these when a view returns
// VIEW_NONE means "stop the dispatcher" (exit app)
static uint32_t nav_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t nav_remote(void* context) {
    UNUSED(context);
    return ViewIdRemote;
}

// BLE connection state callback — called by Bt service when connection changes
static void bt_status_changed(BtStatus status, void* context) {
    App* app = context;
    if(status == BtStatusConnected) {
        app->ble_state = BleStateConnected;
        notification_internal_message(app->notifications, &sequence_set_blue_255);
    } else {
        app->ble_state = BleStateAdvertising;
        notification_internal_message(app->notifications, &sequence_reset_blue);
    }
    // Trigger redraw on the remote view so status updates
    remote_view_set_ble_state(app->remote_view, app->ble_state);
}

static App* app_alloc(void) {
    App* app = malloc(sizeof(App));

    // Open system records
    app->gui = furi_record_open(RECORD_GUI);
    app->bt = furi_record_open(RECORD_BT);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Load saved settings (or defaults)
    settings_load(&app->settings);

    // Set up ViewDispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Remote view — primary interaction screen
    app->remote_view = remote_view_alloc(app);
    view_set_previous_callback(app->remote_view, nav_exit);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdRemote, app->remote_view);

    // Settings view — submenu for configuration
    app->settings_submenu = settings_view_alloc(app);
    view_set_previous_callback(
        submenu_get_view(app->settings_submenu), nav_remote);
    view_dispatcher_add_view(
        app->view_dispatcher, ViewIdSettings, submenu_get_view(app->settings_submenu));

    app->ble_state = BleStateDisconnected;

    return app;
}

static void app_free(App* app) {
    // Remove views before freeing them
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdRemote);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdSettings);

    remote_view_free(app->remote_view);
    settings_view_free(app->settings_submenu);

    view_dispatcher_free(app->view_dispatcher);

    // Close system records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_BT);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

int32_t apple_tv_remote_app(void* p) {
    UNUSED(p);

    App* app = app_alloc();

    // --- BLE HID setup (following bt_hid app pattern) ---

    // Disconnect any existing BLE connection before switching profiles
    bt_disconnect(app->bt);
    furi_delay_ms(200);

    // Set key storage path so pairing bonds persist between launches
    bt_keys_storage_set_storage_path(app->bt, APP_DATA_PATH(BT_KEYS_STORAGE_NAME));

    // Switch BLE profile from Serial (default) to HID Keyboard
    if(!bt_set_profile(app->bt, BtProfileHidKeyboard)) {
        FURI_LOG_E(TAG, "Failed to switch to HID profile");
    }

    // Start BLE advertising so the Apple TV can discover us
    furi_hal_bt_start_advertising();
    app->ble_state = BleStateAdvertising;
    remote_view_set_ble_state(app->remote_view, app->ble_state);

    // Register for connection state changes
    bt_set_status_changed_callback(app->bt, bt_status_changed, app);

    // Start on the remote view
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdRemote);

    // Run the UI event loop — blocks until app exits
    view_dispatcher_run(app->view_dispatcher);

    // --- Cleanup: restore BLE to default state ---

    bt_set_status_changed_callback(app->bt, NULL, NULL);
    bt_disconnect(app->bt);
    furi_delay_ms(200);
    bt_keys_storage_set_default_path(app->bt);

    // Restore Serial profile so the Flipper mobile app works again
    if(!bt_set_profile(app->bt, BtProfileSerial)) {
        FURI_LOG_E(TAG, "Failed to restore Serial profile");
    }

    // Save settings before exit
    settings_save(&app->settings);

    app_free(app);
    return 0;
}
```

- [ ] **Step 2: Commit**

```bash
git add apple_tv_remote.c
git commit -m "feat: app entry point with BLE HID lifecycle"
```

---

### Task 3: Settings persistence

**Files:**
- Create: `settings/settings_storage.h`
- Create: `settings/settings_storage.c`

- [ ] **Step 1: Create `settings/settings_storage.h`**

```c
#pragma once

#include "apple_tv_remote.h"
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#define SETTINGS_FILE_PATH APP_DATA_PATH("config.save")
#define SETTINGS_FILE_TYPE "Apple TV Remote Config"
#define SETTINGS_FILE_VERSION 1
#define DEFAULT_DEVICE_NAME "FlipperTV Remote"

// Load settings from SD card. Populates defaults if file doesn't exist.
void settings_load(AppSettings* settings);

// Save current settings to SD card.
void settings_save(const AppSettings* settings);
```

- [ ] **Step 2: Create `settings/settings_storage.c`**

```c
#include "settings_storage.h"
#include <string.h>

void settings_load(AppSettings* settings) {
    // Start with defaults
    strlcpy(settings->device_name, DEFAULT_DEVICE_NAME, sizeof(settings->device_name));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    if(flipper_format_file_open_existing(ff, SETTINGS_FILE_PATH)) {
        FuriString* filetype = furi_string_alloc();
        uint32_t version = 0;

        if(flipper_format_read_header(ff, filetype, &version) &&
           !furi_string_cmp_str(filetype, SETTINGS_FILE_TYPE) &&
           version == SETTINGS_FILE_VERSION) {
            // Read device name
            FuriString* name = furi_string_alloc();
            if(flipper_format_read_string(ff, "DeviceName", name)) {
                strlcpy(
                    settings->device_name,
                    furi_string_get_cstr(name),
                    sizeof(settings->device_name));
            }
            furi_string_free(name);
        }

        furi_string_free(filetype);
    }

    flipper_format_file_close(ff);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

void settings_save(const AppSettings* settings) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    if(flipper_format_file_open_always(ff, SETTINGS_FILE_PATH)) {
        flipper_format_write_header_cstr(ff, SETTINGS_FILE_TYPE, SETTINGS_FILE_VERSION);
        flipper_format_write_string_cstr(ff, "DeviceName", settings->device_name);
    }

    flipper_format_file_close(ff);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}
```

- [ ] **Step 3: Commit**

```bash
git add settings/
git commit -m "feat: settings persistence via FlipperFormat"
```

---

## Chunk 2: Remote View — Input Handling & Drawing

### Task 4: Remote view — model, alloc, draw callback

**Files:**
- Create: `views/remote_view.h`
- Create: `views/remote_view.c`

- [ ] **Step 1: Create `views/remote_view.h`**

```c
#pragma once

#include "apple_tv_remote.h"

// Forward declare — App is defined in apple_tv_remote.h
typedef struct App App;

// Allocate the remote view. Caller owns the returned View*.
View* remote_view_alloc(App* app);

// Free the remote view and its internal state.
void remote_view_free(View* view);

// Update the BLE connection state displayed on screen.
void remote_view_set_ble_state(View* view, BleState state);
```

- [ ] **Step 2: Create `views/remote_view.c` — model and draw callback**

Start with the draw callback only (no input handling yet). This lets us verify the UI renders before wiring up inputs.

```c
#include "remote_view.h"
#include <gui/elements.h>

// View model — holds state needed for drawing
typedef struct {
    BleState ble_state;
    bool volume_active;
    bool up_pressed;
    bool down_pressed;
    bool left_pressed;
    bool right_pressed;
    bool ok_pressed;
    bool back_pressed;
} RemoteViewModel;

// Internal state not visible to the model (timers, app pointer)
typedef struct {
    App* app;
    View* view; // back-pointer for model access in input callback
    FuriTimer* volume_timer;
    InputKey volume_key; // which key triggered volume mode (Up or Down)
} RemoteViewState;

static void remote_view_draw_callback(Canvas* canvas, void* model) {
    RemoteViewModel* m = model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "FlipperTV Remote");

    // Connection status
    canvas_set_font(canvas, FontSecondary);
    switch(m->ble_state) {
    case BleStateDisconnected:
        canvas_draw_str(canvas, 2, 24, "BLE: Not connected");
        break;
    case BleStateAdvertising:
        canvas_draw_str(canvas, 2, 24, "BLE: Searching...");
        break;
    case BleStateConnected:
        canvas_draw_str(canvas, 2, 24, "BLE: Connected");
        break;
    }

    // D-pad visual — center of screen
    // Up arrow
    canvas_draw_str(canvas, 58, 34, m->up_pressed ? "[^]" : " ^ ");
    // Left / OK / Right
    canvas_draw_str(canvas, 40, 44, m->left_pressed ? "[<]" : " < ");
    canvas_draw_str(canvas, 56, 44, m->ok_pressed ? "[O]" : " O ");
    canvas_draw_str(canvas, 72, 44, m->right_pressed ? "[>]" : " > ");
    // Down arrow
    canvas_draw_str(canvas, 58, 54, m->down_pressed ? "[v]" : " v ");

    // Volume indicator overlay
    if(m->volume_active) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 90, 34, "VOL");
    }

    // Bottom bar hints
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 62, "Back:Menu  Hold OK:Set");
}

View* remote_view_alloc(App* app) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLocking, sizeof(RemoteViewModel));
    view_set_draw_callback(view, remote_view_draw_callback);

    // Allocate internal state and attach to view context
    RemoteViewState* state = malloc(sizeof(RemoteViewState));
    state->app = app;
    state->view = view;
    state->volume_key = InputKeyUp;
    state->volume_timer = NULL; // created in Task 6
    view_set_context(view, state);

    // Initialize model defaults
    with_view_model(
        view,
        RemoteViewModel * model,
        {
            model->ble_state = BleStateDisconnected;
            model->volume_active = false;
            model->up_pressed = false;
            model->down_pressed = false;
            model->left_pressed = false;
            model->right_pressed = false;
            model->ok_pressed = false;
            model->back_pressed = false;
        },
        true);

    return view;
}

void remote_view_free(View* view) {
    RemoteViewState* state = view_get_context(view);
    if(state->volume_timer) {
        furi_timer_stop(state->volume_timer);
        furi_timer_free(state->volume_timer);
    }
    free(state);
    view_free(view);
}

void remote_view_set_ble_state(View* view, BleState ble_state) {
    with_view_model(
        view,
        RemoteViewModel * model,
        { model->ble_state = ble_state; },
        true);
}
```

- [ ] **Step 3: Commit**

```bash
git add views/
git commit -m "feat: remote view with draw callback and status display"
```

---

### Task 5: Remote view — navigation input (short presses)

**Files:**
- Modify: `views/remote_view.c`

- [ ] **Step 1: Add navigation HID helpers and input callback**

Add these functions to `remote_view.c`, above `remote_view_alloc`:

```c
// Send HID key press for navigation
static void send_nav_press(InputKey key) {
    switch(key) {
    case InputKeyUp:
        furi_hal_bt_hid_kb_press(HID_KEYBOARD_UP_ARROW);
        break;
    case InputKeyDown:
        furi_hal_bt_hid_kb_press(HID_KEYBOARD_DOWN_ARROW);
        break;
    case InputKeyLeft:
        furi_hal_bt_hid_kb_press(HID_KEYBOARD_LEFT_ARROW);
        break;
    case InputKeyRight:
        furi_hal_bt_hid_kb_press(HID_KEYBOARD_RIGHT_ARROW);
        break;
    case InputKeyOk:
        furi_hal_bt_hid_kb_press(HID_KEYBOARD_RETURN);
        break;
    default:
        break;
    }
}

// Release HID key
static void send_nav_release(InputKey key) {
    switch(key) {
    case InputKeyUp:
        furi_hal_bt_hid_kb_release(HID_KEYBOARD_UP_ARROW);
        break;
    case InputKeyDown:
        furi_hal_bt_hid_kb_release(HID_KEYBOARD_DOWN_ARROW);
        break;
    case InputKeyLeft:
        furi_hal_bt_hid_kb_release(HID_KEYBOARD_LEFT_ARROW);
        break;
    case InputKeyRight:
        furi_hal_bt_hid_kb_release(HID_KEYBOARD_RIGHT_ARROW);
        break;
    case InputKeyOk:
        furi_hal_bt_hid_kb_release(HID_KEYBOARD_RETURN);
        break;
    default:
        break;
    }
}

// Update the model's pressed state for a key (for visual feedback)
static void set_key_pressed(View* view, InputKey key, bool pressed) {
    with_view_model(
        view,
        RemoteViewModel * model,
        {
            switch(key) {
            case InputKeyUp:
                model->up_pressed = pressed;
                break;
            case InputKeyDown:
                model->down_pressed = pressed;
                break;
            case InputKeyLeft:
                model->left_pressed = pressed;
                break;
            case InputKeyRight:
                model->right_pressed = pressed;
                break;
            case InputKeyOk:
                model->ok_pressed = pressed;
                break;
            case InputKeyBack:
                model->back_pressed = pressed;
                break;
            default:
                break;
            }
        },
        true);
}

static bool remote_view_input_callback(InputEvent* event, void* context) {
    RemoteViewState* state = context;
    View* view = state->view;
    bool consumed = false;

    if(event->type == InputTypePress) {
        // Visual feedback — mark key as pressed
        set_key_pressed(view, event->key, true);

        // Send HID key-down for navigation keys
        if(event->key != InputKeyBack) {
            send_nav_press(event->key);
        }
        consumed = true;

    } else if(event->type == InputTypeRelease) {
        // Visual feedback — mark key as released
        set_key_pressed(view, event->key, false);

        // Send HID key-up for navigation keys
        if(event->key != InputKeyBack) {
            send_nav_release(event->key);
        }
        consumed = true;

    } else if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            // Short back = send Menu/Escape to Apple TV
            furi_hal_bt_hid_kb_press(HID_KEYBOARD_ESCAPE);
            furi_hal_bt_hid_kb_release(HID_KEYBOARD_ESCAPE);
            consumed = true;
        }

    } else if(event->type == InputTypeLong) {
        if(event->key == InputKeyBack) {
            // Long back = exit app — return false to let ViewDispatcher handle it
            furi_hal_bt_hid_kb_release_all();
            furi_hal_bt_hid_consumer_key_release_all();
            consumed = false;
        } else if(event->key == InputKeyOk) {
            // Long OK = open settings
            view_dispatcher_switch_to_view(
                state->app->view_dispatcher, ViewIdSettings);
            consumed = true;
        }
    }

    return consumed;
}
```

- [ ] **Step 2: Wire the input callback in `remote_view_alloc`**

Add after `view_set_draw_callback(view, remote_view_draw_callback);`:

```c
    view_set_input_callback(view, remote_view_input_callback);
```

- [ ] **Step 3: Commit**

```bash
git add views/remote_view.c
git commit -m "feat: navigation input handling with HID key press/release"
```

---

### Task 6: Remote view — volume long-press with timer

**Files:**
- Modify: `views/remote_view.c`

- [ ] **Step 1: Add the volume timer callback**

Add above the input callback in `remote_view.c`:

```c
// Timer callback — fires every VOLUME_REPEAT_INTERVAL_MS while volume is active
static void volume_timer_callback(void* context) {
    RemoteViewState* state = context;

    // Send the appropriate volume command
    uint16_t key = (state->volume_key == InputKeyUp)
        ? HID_CONSUMER_VOLUME_INCREMENT
        : HID_CONSUMER_VOLUME_DECREMENT;

    furi_hal_bt_hid_consumer_key_press(key);
    furi_hal_bt_hid_consumer_key_release(key);
}
```

- [ ] **Step 2: Create the timer in `remote_view_alloc`**

Replace `state->volume_timer = NULL; // created in Task 6` with:

```c
    state->volume_timer = furi_timer_alloc(
        volume_timer_callback, FuriTimerTypePeriodic, state);
```

- [ ] **Step 3: Add volume activation to InputTypeLong handler**

Replace the `InputTypeLong` branch in `remote_view_input_callback` with:

```c
    } else if(event->type == InputTypeLong) {
        if(event->key == InputKeyBack) {
            // Long back = exit app — return false to let ViewDispatcher handle it
            furi_hal_bt_hid_kb_release_all();
            furi_hal_bt_hid_consumer_key_release_all();
            consumed = false;
        } else if(event->key == InputKeyOk) {
            // Long OK = open settings
            view_dispatcher_switch_to_view(
                state->app->view_dispatcher, ViewIdSettings);
            consumed = true;
        } else if(event->key == InputKeyUp || event->key == InputKeyDown) {
            // Long up/down = enter volume mode
            // Release the navigation key first (it was pressed on InputTypePress)
            send_nav_release(event->key);

            state->volume_key = event->key;
            with_view_model(
                state->view,
                RemoteViewModel * model,
                { model->volume_active = true; },
                true);

            // Send first volume command immediately
            volume_timer_callback(state);

            // Start periodic repeat
            furi_timer_start(state->volume_timer, VOLUME_REPEAT_INTERVAL_MS);
            consumed = true;
        }
    }
```

- [ ] **Step 4: Add volume stop on Release**

Replace the `InputTypeRelease` branch with:

```c
    } else if(event->type == InputTypeRelease) {
        set_key_pressed(view, event->key, false);

        if(event->key != InputKeyBack) {
            // Check if we were in volume mode for up/down
            bool was_volume = false;
            if((event->key == InputKeyUp || event->key == InputKeyDown) &&
               state->volume_timer) {
                furi_timer_stop(state->volume_timer);
                with_view_model(
                    state->view,
                    RemoteViewModel * model,
                    {
                        was_volume = model->volume_active;
                        model->volume_active = false;
                    },
                    true);
                if(was_volume) {
                    // Volume mode was active — consumer key cleanup only,
                    // nav key was already released when volume started
                    furi_hal_bt_hid_consumer_key_release_all();
                }
            }

            // Only release nav key if we weren't in volume mode
            // (volume activation already released the nav key)
            if(!was_volume) {
                send_nav_release(event->key);
            }
        }
        consumed = true;
    }
```

- [ ] **Step 5: Commit**

```bash
git add views/remote_view.c
git commit -m "feat: volume control via long-press up/down with 400ms repeat"
```

---

## Chunk 3: Settings View & App Icon

### Task 7: Settings submenu view

**Files:**
- Create: `views/settings_view.h`
- Create: `views/settings_view.c`

- [ ] **Step 1: Create `views/settings_view.h`**

```c
#pragma once

#include "apple_tv_remote.h"

typedef struct App App;

// Allocate the settings submenu. Returns a Submenu* (Flipper GUI module).
Submenu* settings_view_alloc(App* app);

// Free the settings submenu.
void settings_view_free(Submenu* submenu);
```

- [ ] **Step 2: Create `views/settings_view.c`**

```c
#include "settings_view.h"
#include "remote_view.h"
#include <storage/storage.h>

typedef enum {
    SettingsIndexForgetPairing,
} SettingsIndex;

static void settings_submenu_callback(void* context, uint32_t index) {
    App* app = context;

    switch(index) {
    case SettingsIndexForgetPairing:
        // Clear stored BLE bond keys
        bt_disconnect(app->bt);
        furi_delay_ms(200);

        // Delete the key storage file to forget pairing
        {
            Storage* storage = furi_record_open(RECORD_STORAGE);
            storage_simply_remove(storage, APP_DATA_PATH(BT_KEYS_STORAGE_NAME));
            furi_record_close(RECORD_STORAGE);
        }

        // Restart advertising so user can re-pair
        furi_hal_bt_start_advertising();
        app->ble_state = BleStateAdvertising;
        remote_view_set_ble_state(app->remote_view, app->ble_state);

        // Go back to remote view
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdRemote);
        break;
    }
}

Submenu* settings_view_alloc(App* app) {
    Submenu* submenu = submenu_alloc();
    submenu_add_item(
        submenu, "Forget Pairing", SettingsIndexForgetPairing,
        settings_submenu_callback, app);
    return submenu;
}

void settings_view_free(Submenu* submenu) {
    submenu_free(submenu);
}
```

- [ ] **Step 3: Commit**

```bash
git add views/settings_view.h views/settings_view.c
git commit -m "feat: settings submenu with forget pairing option"
```

---

### Task 8: App icon

**Files:**
- Create: `assets/icons/remote_10px.png`

- [ ] **Step 1: Create a 10x10 1-bit PNG app icon**

The icon needs to be 10x10 pixels, 1-bit color depth (black and white). Create a simple remote/play button icon using ImageMagick:

```bash
cd flipper-apple-tv-remote
mkdir -p assets/icons

# Create a 10x10 play-button triangle icon (1-bit PNG)
convert -size 10x10 xc:white \
    -fill black \
    -draw "polygon 3,1 3,8 8,4" \
    -type Bilevel \
    assets/icons/remote_10px.png
```

If ImageMagick isn't available, create any 10x10 1-bit PNG as a placeholder. The icon can be refined later.

- [ ] **Step 2: Commit**

```bash
git add assets/
git commit -m "feat: add 10x10 app icon"
```

---

### Task 9: SPEC.md and PROJECT_LOG.md

**Files:**
- Create: `SPEC.md`
- Create: `PROJECT_LOG.md`

- [ ] **Step 1: Create `SPEC.md`**

```markdown
# Apple TV Remote — Specification

Full design spec: [docs/superpowers/specs/2026-03-13-apple-tv-remote-design.md](docs/superpowers/specs/2026-03-13-apple-tv-remote-design.md)

Implementation plan: [docs/superpowers/plans/2026-03-14-apple-tv-remote-v1.md](docs/superpowers/plans/2026-03-14-apple-tv-remote-v1.md)
```

- [ ] **Step 2: Create `PROJECT_LOG.md`**

```markdown
# Apple TV Remote — Project Log

## 2026-03-14 — Project Kickoff

- Designed BLE HID + IR hybrid remote for Flipper Zero
- v1 scope: BLE HID only (IR is Phase 2)
- Direct D-pad mapping with volume via long-press up/down
- Targeting Momentum firmware, App Catalog submission
- Using ViewDispatcher pattern (no SceneManager — overkill for 2 views)

### Current State
- Design spec complete and reviewed
- Implementation plan written
- No code yet

### Next Steps
- Implement per plan: scaffold -> BLE -> input -> settings -> build & test
```

- [ ] **Step 3: Commit**

```bash
git add SPEC.md PROJECT_LOG.md
git commit -m "docs: add SPEC.md and PROJECT_LOG.md"
```

---

## Chunk 4: Build, Test & Polish

### Task 10: Fix compilation — resolve includes and wire everything together

**Files:**
- Modify: `apple_tv_remote.h` (if needed for forward declarations)
- Modify: `apple_tv_remote.c` (include adjustments)
- Modify: `views/remote_view.c` (include adjustments)

- [ ] **Step 1: Verify include graph is clean**

The include order should be:
1. `apple_tv_remote.h` — defines `App`, `AppSettings`, `BleState`, `ViewId`, constants
2. `settings/settings_storage.h` — includes `apple_tv_remote.h`, declares save/load
3. `views/remote_view.h` — includes `apple_tv_remote.h`, declares view alloc/free
4. `views/settings_view.h` — includes `apple_tv_remote.h`, declares submenu alloc/free
5. `apple_tv_remote.c` — includes all of the above

Check for circular dependencies. The `remote_view.c` needs `view_dispatcher_switch_to_view` for long-press OK (settings). This works because `RemoteViewState` holds an `App*` pointer.

The `settings_view.c` calls `remote_view_set_ble_state` — it includes `remote_view.h` for this.

- [ ] **Step 2: Fix any include issues found, commit**

```bash
git add -A
git commit -m "fix: resolve compilation issues"
```

---

### Task 11: Build and deploy

- [ ] **Step 1: Set up build environment**

```bash
# Clone Momentum firmware if not already present
git clone --recursive https://github.com/Next-Flip/Momentum-Firmware.git ~/Momentum-Firmware

# Symlink our app into the firmware build tree
ln -s "/Users/pickleresistor/Library/Mobile Documents/com~apple~CloudDocs/Developer/flipper-apple-tv-remote" \
    ~/Momentum-Firmware/applications_user/apple_tv_remote

# Build the FAP
cd ~/Momentum-Firmware
./fbt fap_apple_tv_remote
```

- [ ] **Step 2: Deploy to Flipper**

Copy the built `.fap` to the Flipper's SD card:
```bash
# Via qFlipper CLI or direct SD card copy
# Output is at: build/f7-firmware-D/.extapps/apple_tv_remote.fap
# Copy to: /ext/apps/Bluetooth/apple_tv_remote.fap
```

- [ ] **Step 3: Fix build errors if any, commit fixes**

```bash
git add -A
git commit -m "fix: address build errors from Momentum SDK compilation"
```

---

### Task 12: Manual testing protocol

**No code changes — hardware testing with real Apple TV.**

- [ ] **Step 1: Test BLE pairing**

1. Launch app on Flipper — verify "BLE: Searching..." displayed
2. On Apple TV: Settings > Remotes and Devices > Bluetooth
3. Select "FlipperTV Remote" — verify pairing completes
4. Flipper should show "BLE: Connected" + blue LED
5. Exit app (long-press back), relaunch — verify auto-reconnect

- [ ] **Step 2: Test navigation**

1. D-pad Up/Down/Left/Right — verify Apple TV navigation responds
2. OK button — verify Select/Enter works
3. Short-press Back — verify Menu/Back on Apple TV
4. Visual feedback: verify pressed indicators show on Flipper screen

- [ ] **Step 3: Test volume**

1. Hold Up >400ms — volume should increase at ~2.5/sec
2. Release — volume stops immediately
3. Hold Down >400ms — volume should decrease
4. Verify "VOL" indicator on screen during adjustment
5. Verify short tap Up still navigates (no accidental volume)

- [ ] **Step 4: Test settings and edge cases**

1. Long-press OK — settings submenu opens
2. "Forget Pairing" — clears bond, shows "Searching..."
3. Back from settings — returns to remote view
4. Disconnect Apple TV power — verify Flipper shows state change
5. Exit/re-enter app multiple times — no crashes
6. After exiting: verify Flipper mobile app BLE works (Serial profile restored)

- [ ] **Step 5: Document results in PROJECT_LOG.md, commit**

```bash
git add PROJECT_LOG.md
git commit -m "docs: record manual test results"
```

---

### Task 13: Create GitHub repository

- [ ] **Step 1: Create private repo and push**

```bash
cd "/Users/pickleresistor/Library/Mobile Documents/com~apple~CloudDocs/Developer/flipper-apple-tv-remote"
gh repo create 1507-systems/flipper-apple-tv-remote --private --source=. --push
```

- [ ] **Step 2: Verify**

```bash
gh repo view 1507-systems/flipper-apple-tv-remote
```
