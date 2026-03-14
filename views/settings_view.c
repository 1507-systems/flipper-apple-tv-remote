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

        // Re-initialize the HID profile to restart advertising cleanly
        // (using service layer rather than direct HAL call to ensure
        // bond database is properly reset after key deletion)
        bt_set_profile(app->bt, BtProfileHidKeyboard);
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
