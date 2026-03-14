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
        // Intentional simplification: BtStatusOff and other non-connected states
        // are collapsed into Advertising. If the radio is off, the UI will show
        // "Searching..." which is misleading but acceptable for v1.
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
