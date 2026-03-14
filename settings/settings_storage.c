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
    } else {
        FURI_LOG_E("AppleTVRemote", "Failed to save settings");
    }

    flipper_format_file_close(ff);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}
