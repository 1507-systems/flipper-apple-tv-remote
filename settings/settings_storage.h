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
