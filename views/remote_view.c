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

/* ---------- Draw callback ---------- */

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
    canvas_draw_str(canvas, 58, 34, m->up_pressed ? "[^]" : " ^ ");
    canvas_draw_str(canvas, 40, 44, m->left_pressed ? "[<]" : " < ");
    canvas_draw_str(canvas, 56, 44, m->ok_pressed ? "[O]" : " O ");
    canvas_draw_str(canvas, 72, 44, m->right_pressed ? "[>]" : " > ");
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

/* ---------- Volume timer callback ---------- */

// Timer callback — fires every VOLUME_REPEAT_INTERVAL_MS while volume is active
static void volume_timer_callback(void* context) {
    RemoteViewState* state = context;
    uint16_t key = (state->volume_key == InputKeyUp)
        ? HID_CONSUMER_VOLUME_INCREMENT
        : HID_CONSUMER_VOLUME_DECREMENT;
    furi_hal_bt_hid_consumer_key_press(key);
    furi_hal_bt_hid_consumer_key_release(key);
}

/* ---------- Navigation input helpers ---------- */

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

/* ---------- Input callback ---------- */

static bool remote_view_input_callback(InputEvent* event, void* context) {
    RemoteViewState* state = context;
    View* view = state->view;
    bool consumed = false;

    if(event->type == InputTypePress) {
        set_key_pressed(view, event->key, true);
        if(event->key != InputKeyBack) {
            send_nav_press(event->key);
        }
        consumed = true;

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
                    furi_hal_bt_hid_consumer_key_release_all();
                }
            }
            // Only release nav key if we weren't in volume mode
            if(!was_volume) {
                send_nav_release(event->key);
            }
        }
        consumed = true;

    } else if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            furi_hal_bt_hid_kb_press(HID_KEYBOARD_ESCAPE);
            furi_hal_bt_hid_kb_release(HID_KEYBOARD_ESCAPE);
            consumed = true;
        }

    } else if(event->type == InputTypeLong) {
        if(event->key == InputKeyBack) {
            furi_hal_bt_hid_kb_release_all();
            furi_hal_bt_hid_consumer_key_release_all();
            consumed = false; // let ViewDispatcher handle exit
        } else if(event->key == InputKeyOk) {
            view_dispatcher_switch_to_view(
                state->app->view_dispatcher, ViewIdSettings);
            consumed = true;
        } else if(event->key == InputKeyUp || event->key == InputKeyDown) {
            // Release nav key first (was pressed on InputTypePress)
            send_nav_release(event->key);
            state->volume_key = event->key;
            with_view_model(
                state->view,
                RemoteViewModel * model,
                { model->volume_active = true; },
                true);
            volume_timer_callback(state); // send first volume command immediately
            furi_timer_start(state->volume_timer, VOLUME_REPEAT_INTERVAL_MS);
            consumed = true;
        }
    }

    return consumed;
}

/* ---------- Alloc / Free / State setters ---------- */

View* remote_view_alloc(App* app) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLocking, sizeof(RemoteViewModel));
    view_set_draw_callback(view, remote_view_draw_callback);
    view_set_input_callback(view, remote_view_input_callback);

    // Allocate internal state and attach to view context
    RemoteViewState* state = malloc(sizeof(RemoteViewState));
    state->app = app;
    state->view = view;
    state->volume_key = InputKeyUp;
    state->volume_timer = furi_timer_alloc(volume_timer_callback, FuriTimerTypePeriodic, state);
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
