#pragma once

#include "apple_tv_remote.h"

typedef struct App App;

// Allocate the settings submenu. Returns a Submenu* (Flipper GUI module).
Submenu* settings_view_alloc(App* app);

// Free the settings submenu.
void settings_view_free(Submenu* submenu);
