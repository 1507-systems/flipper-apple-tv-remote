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
