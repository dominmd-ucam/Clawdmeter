#pragma once
#include "data.h"
#include "ble.h"

enum screen_t {
    SCREEN_SPLASH,
    SCREEN_USAGE,
    SCREEN_STATUS,
    SCREEN_MUSIC,
    SCREEN_COUNT,
};

void ui_init(void);
void ui_update(const UsageData* data);
void ui_update_music(const MusicData* data);
void ui_tick_anim(void);
void ui_show_screen(screen_t screen);
void ui_cycle_page(void);
void ui_toggle_splash(void);
screen_t ui_get_current_screen(void);
void ui_update_ble_status(ble_state_t state, const char* name, const char* mac);
void ui_update_battery(int percent, bool charging);

// Current local wall-clock time from the daemon-fed clock (the same source that
// drives the on-screen HH:MM). Returns false if the daemon hasn't sent a clock
// yet (nothing to schedule against). Writes hour (0-23) and minute (0-59).
bool ui_get_local_time(int* hour, int* min);
