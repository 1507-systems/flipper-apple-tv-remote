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
// HID keycode defines (HID_KEYBOARD_*, HID_CONSUMER_*) used by remote_view.c
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
