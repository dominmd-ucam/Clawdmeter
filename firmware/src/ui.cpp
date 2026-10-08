#include "ui.h"
#include "splash.h"
#include <lvgl.h>
#include <time.h>
#include "logo.h"
#include "icons.h"
#include "icons_status.h"
#include "icons_services.h"
#include "hal/board_caps.h"
#include "cover.h"
#include <esp_heap_caps.h>

// Custom fonts (scaled for 314 PPI, ~1.9x from original 165 PPI)
LV_FONT_DECLARE(font_tiempos_56);
LV_FONT_DECLARE(font_tiempos_34);
LV_FONT_DECLARE(font_styrene_48);
LV_FONT_DECLARE(font_styrene_28);
LV_FONT_DECLARE(font_styrene_24);
LV_FONT_DECLARE(font_styrene_20);
LV_FONT_DECLARE(font_styrene_16);
LV_FONT_DECLARE(font_styrene_14);
LV_FONT_DECLARE(font_mono_32);

// Layout values computed from the active board's geometry. Populated once
// in ui_init() and treated as const for the rest of the program. Adding a
// new display size means extending compute_layout() with another
// breakpoint — never editing the screen-builder functions below.
struct Layout {
    int16_t scr_w, scr_h;
    int16_t margin;
    int16_t title_y;
    int16_t content_y;
    int16_t content_w;

    // Usage screen
    int16_t usage_panel_h;
    int16_t usage_panel_gap;
    int16_t usage_bar_y;
    int16_t usage_reset_y;

    // Bluetooth screen
    int16_t bt_info_panel_h;
    int16_t bt_reset_zone_h;
    const lv_font_t* bt_title_font;
    const lv_font_t* bt_status_font;
    const lv_font_t* bt_device_font;
    const lv_font_t* bt_credit_1_font;
    const lv_font_t* bt_credit_2_font;

    // Music screen (cover on top, text, progress, transport buttons)
    int16_t music_cover;        // square cover side
    int16_t music_cover_y;
    int16_t music_btn;          // prev/next button diameter
    int16_t music_btn_play;     // play/pause button diameter
    const lv_font_t* music_title_font;
    const lv_font_t* music_artist_font;
    const lv_font_t* music_time_font;
    const lv_font_t* music_icon_font;
    const lv_font_t* music_play_font;
};
static Layout L = {};

// Pick layout values from the active board's pixel dimensions. The two
// existing boards happen to land on the two breakpoints below; new ports
// inherit the closer one — visually OK, may need a polish pass for
// pixel-perfect alignment but never blocks the port from booting.
static void compute_layout(const BoardCaps& c) {
    L.scr_w = c.width;
    L.scr_h = c.height;
    L.margin = 20;
    L.title_y = 30;

    if (c.height >= 460) {
        // Large layout — tuned for 480x480 (AMOLED-2.16).
        L.content_y = 100;
        L.usage_panel_h = 150;
        L.usage_panel_gap = 16;
        L.usage_bar_y = 56;
        L.usage_reset_y = 94;
        L.bt_info_panel_h = 160;
        L.bt_reset_zone_h = 110;
        L.bt_title_font    = &font_tiempos_56;
        L.bt_status_font   = &font_styrene_48;
        L.bt_device_font   = &font_styrene_28;
        L.bt_credit_1_font = &font_styrene_24;
        L.bt_credit_2_font = &font_styrene_20;
        L.music_cover       = 200;
        L.music_cover_y     = 30;
        L.music_btn         = 68;
        L.music_btn_play    = 88;
        L.music_title_font  = &font_styrene_28;
        L.music_artist_font = &font_styrene_20;
        L.music_time_font   = &font_styrene_16;
        L.music_icon_font   = &lv_font_montserrat_32;
        L.music_play_font   = &lv_font_montserrat_40;
    } else {
        // Compact layout — tuned for 368x448 (AMOLED-1.8).
        L.content_y = 85;
        L.usage_panel_h = 130;
        L.usage_panel_gap = 12;
        L.usage_bar_y = 48;
        L.usage_reset_y = 78;
        L.bt_info_panel_h = 140;
        L.bt_reset_zone_h = 90;
        L.bt_title_font    = &font_tiempos_34;
        L.bt_status_font   = &font_styrene_28;
        L.bt_device_font   = &font_styrene_20;
        L.bt_credit_1_font = &font_styrene_16;
        L.bt_credit_2_font = &font_styrene_14;
        L.music_cover       = 150;
        L.music_cover_y     = 64;   // below the battery label on the narrower panel
        L.music_btn         = 60;
        L.music_btn_play    = 76;
        L.music_title_font  = &font_styrene_24;
        L.music_artist_font = &font_styrene_16;
        L.music_time_font   = &font_styrene_14;
        L.music_icon_font   = &lv_font_montserrat_32;
        L.music_play_font   = &lv_font_montserrat_32;
    }

    L.content_w = L.scr_w - 2 * L.margin;
}

// Anthropic brand palette — design tokens live in theme.h
#include "theme.h"
#define COL_BG        THEME_BG
#define COL_PANEL     THEME_PANEL
#define COL_TEXT      THEME_TEXT
#define COL_DIM       THEME_DIM
#define COL_ACCENT    THEME_ACCENT
#define COL_GREEN     THEME_GREEN
#define COL_AMBER     THEME_AMBER
#define COL_RED       THEME_RED
#define COL_BAR_BG    THEME_BAR_BG

// ---- Usage screen widgets (single non-splash view) ----
static lv_obj_t* usage_container;
static lv_obj_t* lbl_title;
// Clock fed by the daemon: base epoch (local wall-clock seconds) + the lv_tick at
// which it landed, so the title ticks forward locally between 60s payloads.
static long     clock_base_epoch = 0;
static uint32_t clock_base_ms = 0;
static int      clock_fmt = 24;   // 12 or 24, set from the daemon payload
static int      clock_last_min = -1;   // last rendered minute; avoids redrawing the title every tick
static lv_obj_t* usage_group;   // the two usage panels — shown when connected
static lv_obj_t* pair_group;    // pairing hint — shown when disconnected
static lv_obj_t* bar_session;
static lv_obj_t* lbl_session_pct;
static lv_obj_t* lbl_session_label;
static lv_obj_t* lbl_session_reset;
static lv_obj_t* bar_weekly;
static lv_obj_t* lbl_weekly_pct;
static lv_obj_t* lbl_weekly_label;
static lv_obj_t* lbl_weekly_reset;
static lv_obj_t* panel_session = nullptr;
static lv_obj_t* panel_weekly = nullptr;
// Enterprise-only widgets inside panel_session
static lv_obj_t* lbl_session_pct_sym = nullptr;  // "%" in smaller font
static lv_obj_t* lbl_spending_desc = nullptr;     // "of your monthly budget"
static lv_obj_t* lbl_spending_status = nullptr;   // "Under pace" / "On pace" / "Over pace"
static lv_obj_t* lbl_anim;      // status line: connection state + whimsical idle

// ---- Status screen widgets (clock / weather / agent dots / tomorrow) ----
static lv_obj_t* status_container = nullptr;
static lv_obj_t* lbl_status_clock = nullptr;   // big HH:MM, top-center
static lv_obj_t* lbl_weather_temp = nullptr;   // big temperature number
static lv_obj_t* deg_ring = nullptr;           // hollow ring = degree mark (fonts are ASCII-only)
static lv_obj_t* lbl_weather_cond = nullptr;   // condition word
static lv_obj_t* lbl_tmr_high = nullptr;       // tomorrow high-temp number
static lv_obj_t* deg_ring_tmr = nullptr;       // tomorrow degree-mark ring
static lv_obj_t* lbl_tmr_cond = nullptr;       // tomorrow condition word
static lv_obj_t* img_tmr = nullptr;            // tomorrow condition icon (reuses weather_dscs)
static lv_obj_t* agent_dots[5] = {};           // 5 health dots (green=alive, red=down)
static lv_obj_t* img_flag = nullptr;           // Union Jack next to "London"
static lv_obj_t* img_weather = nullptr;        // condition icon (swapped by wc)
static lv_image_dsc_t flag_dsc;
static lv_image_dsc_t weather_dscs[7];         // WI_* icons
static lv_image_dsc_t agent_icon_dscs[5];      // per-agent identity icons

// ---- Music screen widgets (now playing on the host, via the daemon) ----
static lv_obj_t* music_container = nullptr;
static lv_obj_t* music_cover = nullptr;        // placeholder panel; the cover image goes inside
static lv_obj_t* lbl_music_title = nullptr;
static lv_obj_t* lbl_music_artist = nullptr;
static lv_obj_t* bar_music = nullptr;
static lv_obj_t* lbl_music_pos = nullptr;
static lv_obj_t* lbl_music_dur = nullptr;
static lv_obj_t* lbl_music_play = nullptr;     // play/pause glyph inside the big button
static lv_obj_t* music_note = nullptr;         // placeholder glyph while there is no cover
static lv_obj_t* img_music_cover = nullptr;
static lv_image_dsc_t cover_dsc;
static uint16_t* cover_px = nullptr;           // decoded cover, RGB565, music_cover² px (PSRAM)
static uint16_t  cover_id = 0;                 // art id currently held in cover_px; 0 = none
static MusicData s_music = {};
static uint32_t  music_rx_ms = 0;              // lv_tick when s_music landed (progress extrapolation)
static uint32_t  music_last_draw_ms = 0;
static int       music_shown_active = -1;      // -1 unknown / 0 "nothing playing" / 1 track
static const uint32_t MUSIC_FRESH_MS = 20000;  // daemon re-sends every few seconds while connected

// ---- Animated top-left logo (claudepix creature, both pages) ----
static splash_sprite_t logo_sprite = {};
static lv_obj_t* agent_hl[5] = {};              // orange highlight block behind a working agent
static bool      s_agent_busy[5] = {};          // mirror of agent_busy, for the pulse tick

// ---- Battery indicator (shared, on top) ----
static lv_obj_t* battery_img;
static lv_obj_t* lbl_batt_pct = nullptr;
static lv_obj_t* logo_img;
static lv_image_dsc_t battery_dscs[5];  // empty, low, medium, full, charging

// ---- Live-data freshness → which usage sub-view to show ----
// usage panels when data is flowing, an idle "Zzz" screen when the host is
// connected but no usage update landed within DATA_FRESH_MS, the pairing hint
// when BLE is down. Re-evaluated every loop in ui_tick_anim().
static lv_obj_t* idle_group;            // the "Zzz" idle screen
static uint32_t  last_data_ms = 0;      // lv_tick when the last valid usage update landed
static bool      data_received = false; // any valid update since boot
static int       view_state = -1;       // -1 unknown / 0 pair / 1 idle / 2 usage
static const uint32_t DATA_FRESH_MS = 90000;  // usage counts as "live" within this window (daemon sends ~60s)

// ---- Shared ----
static lv_image_dsc_t logo_dsc;
static screen_t current_screen = SCREEN_USAGE;
static bool     s_ble_connected = false;   // cached BLE connection state
static uint32_t connected_at_ms = 0;       // when we last entered CONNECTED ("Connected" dwell)

// Animation state
static uint32_t anim_last_ms = 0;
static uint8_t anim_spinner_idx = 0;
static uint8_t anim_phase = 0;
static uint8_t anim_msg_idx = 0;
static uint32_t anim_msg_start = 0;
#define ANIM_MSG_MS     4000

static const char* const spinner_frames[] = {
    "\xC2\xB7", "\xE2\x9C\xBB", "\xE2\x9C\xBD",
    "\xE2\x9C\xB6", "\xE2\x9C\xB3", "\xE2\x9C\xA2",
};
#define SPINNER_COUNT 6
#define SPINNER_PHASES (2 * (SPINNER_COUNT - 1))  // 10: ping-pong 0..5..0

static const uint16_t spinner_ms[SPINNER_COUNT] = {
    260, 130, 130, 130, 130, 260,
};

static const char* const anim_messages[] = {
    "Accomplishing", "Elucidating", "Perusing",
    "Actioning", "Enchanting", "Philosophising",
    "Actualizing", "Envisioning", "Pondering",
    "Baking", "Finagling", "Pontificating",
    "Booping", "Flibbertigibbeting", "Processing",
    "Brewing", "Forging", "Puttering",
    "Calculating", "Forming", "Puzzling",
    "Cerebrating", "Frolicking", "Reticulating",
    "Channelling", "Generating", "Ruminating",
    "Churning", "Germinating", "Scheming",
    "Clauding", "Hatching", "Schlepping",
    "Coalescing", "Herding", "Shimmying",
    "Cogitating", "Honking", "Shucking",
    "Combobulating", "Hustling", "Simmering",
    "Computing", "Ideating", "Smooshing",
    "Concocting", "Imagining", "Spelunking",
    "Conjuring", "Incubating", "Spinning",
    "Considering", "Inferring", "Stewing",
    "Contemplating", "Jiving", "Sussing",
    "Cooking", "Manifesting", "Synthesizing",
    "Crafting", "Marinating", "Thinking",
    "Creating", "Meandering", "Tinkering",
    "Crunching", "Moseying", "Transmuting",
    "Deciphering", "Mulling", "Unfurling",
    "Deliberating", "Mustering", "Unravelling",
    "Determining", "Musing", "Vibing",
    "Discombobulating", "Noodling", "Wandering",
    "Divining", "Percolating", "Whirring",
    "Doing", "Wibbling",
    "Effecting", "Wizarding",
    "Working", "Wrangling",
};
#define ANIM_MSG_COUNT (sizeof(anim_messages) / sizeof(anim_messages[0]))

static lv_color_t pct_color(float pct) {
    if (pct >= 80.0f) return COL_RED;
    if (pct >= 50.0f) return COL_AMBER;
    return COL_GREEN;
}

static void format_reset_time(int mins, char* buf, size_t len) {
    if (mins < 0) {
        snprintf(buf, len, "---");
    } else if (mins < 60) {
        snprintf(buf, len, "Resets in %dm", mins);
    } else if (mins < 1440) {
        snprintf(buf, len, "Resets in %dh %dm", mins / 60, mins % 60);
    } else {
        snprintf(buf, len, "Resets in %dd %dh", mins / 1440, (mins % 1440) / 60);
    }
}

// Forward decls — callbacks defined near ui_show_screen below
static void global_click_cb(lv_event_t* e);

static lv_obj_t* make_panel(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_style_bg_color(panel, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_left(panel, 16, 0);
    lv_obj_set_style_pad_right(panel, 16, 0);
    lv_obj_set_style_pad_top(panel, 12, 0);
    lv_obj_set_style_pad_bottom(panel, 12, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_EVENT_BUBBLE);
    return panel;
}

static lv_obj_t* make_bar(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* bar = lv_bar_create(parent);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, h);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, COL_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, COL_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 6, LV_PART_INDICATOR);
    return bar;
}

static void init_icon_dsc_rgb565a8(lv_image_dsc_t* dsc, int w, int h, const uint8_t* data) {
    dsc->header.w = w;
    dsc->header.h = h;
    dsc->header.cf = LV_COLOR_FORMAT_RGB565A8;
    dsc->header.stride = w * 2;
    dsc->data = data;
    dsc->data_size = w * h * 3;
}

static lv_obj_t* make_pill(lv_obj_t* parent, const char* text) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &font_styrene_28, 0);
    lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
    lv_obj_set_style_bg_color(lbl, COL_BAR_BG, 0);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(lbl, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_left(lbl, 18, 0);
    lv_obj_set_style_pad_right(lbl, 18, 0);
    lv_obj_set_style_pad_top(lbl, 6, 0);
    lv_obj_set_style_pad_bottom(lbl, 6, 0);
    return lbl;
}

static void init_battery_icons(void) {
    init_icon_dsc_rgb565a8(&battery_dscs[0], ICON_BATTERY_W, ICON_BATTERY_H, icon_battery_data);
    init_icon_dsc_rgb565a8(&battery_dscs[1], ICON_BATTERY_LOW_W, ICON_BATTERY_LOW_H, icon_battery_low_data);
    init_icon_dsc_rgb565a8(&battery_dscs[2], ICON_BATTERY_MEDIUM_W, ICON_BATTERY_MEDIUM_H, icon_battery_medium_data);
    init_icon_dsc_rgb565a8(&battery_dscs[3], ICON_BATTERY_FULL_W, ICON_BATTERY_FULL_H, icon_battery_full_data);
    init_icon_dsc_rgb565a8(&battery_dscs[4], ICON_BATTERY_CHARGING_W, ICON_BATTERY_CHARGING_H, icon_battery_charging_data);
}

// ======== Usage Screen ========

static lv_obj_t* make_usage_panel(lv_obj_t* parent, int y, const char* pill_text,
                                  lv_obj_t** out_pct, lv_obj_t** out_pill,
                                  lv_obj_t** out_bar, lv_obj_t** out_reset) {
    lv_obj_t* panel = make_panel(parent, L.margin, y, L.content_w, L.usage_panel_h);

    *out_pct = lv_label_create(panel);
    lv_label_set_text(*out_pct, "---%");
    lv_obj_set_style_text_font(*out_pct, &font_styrene_48, 0);
    lv_obj_set_style_text_color(*out_pct, COL_TEXT, 0);
    lv_obj_set_pos(*out_pct, 0, 0);

    *out_pill = make_pill(panel, pill_text);
    lv_obj_align(*out_pill, LV_ALIGN_TOP_RIGHT, 0, 1);

    *out_bar = make_bar(panel, 0, L.usage_bar_y, L.content_w - 32, 24);

    *out_reset = lv_label_create(panel);
    lv_label_set_text(*out_reset, "---");
    lv_obj_set_style_text_font(*out_reset, &font_styrene_28, 0);
    lv_obj_set_style_text_color(*out_reset, COL_DIM, 0);
    lv_obj_set_pos(*out_reset, 0, L.usage_reset_y);

    return panel;
}

// Pairing hint — shown when disconnected so the screen isn't empty and the
// user knows how to (re)pair. Wording matches the 3-second release gesture.
static void build_pair_group(lv_obj_t* parent) {
    pair_group = lv_obj_create(parent);
    lv_obj_set_size(pair_group, L.scr_w, L.scr_h - L.content_y);
    lv_obj_set_pos(pair_group, 0, L.content_y);
    lv_obj_set_style_bg_opa(pair_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pair_group, 0, 0);
    lv_obj_set_style_pad_all(pair_group, 0, 0);
    lv_obj_clear_flag(pair_group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(pair_group, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t* l1 = lv_label_create(pair_group);
    lv_label_set_text(l1, "To pair");
    lv_obj_set_style_text_font(l1, L.bt_status_font, 0);
    lv_obj_set_style_text_color(l1, COL_TEXT, 0);
    lv_obj_align(l1, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t* l2 = lv_label_create(pair_group);
    lv_label_set_text(l2, "hold the power button");
    lv_obj_set_style_text_font(l2, L.bt_device_font, 0);
    lv_obj_set_style_text_color(l2, COL_DIM, 0);
    lv_obj_align(l2, LV_ALIGN_TOP_MID, 0, 120);

    lv_obj_t* l3 = lv_label_create(pair_group);
    lv_label_set_text(l3, "for 3 seconds, then release");
    lv_obj_set_style_text_font(l3, L.bt_device_font, 0);
    lv_obj_set_style_text_color(l3, COL_DIM, 0);
    lv_obj_align(l3, LV_ALIGN_TOP_MID, 0, 160);

    lv_obj_add_flag(pair_group, LV_OBJ_FLAG_HIDDEN);  // ui_update_ble_status decides
}

// Idle "Zzz" screen — shown when the host is connected but no usage update has
// landed recently (token expired, daemon down, host asleep…). Full-screen, like
// the pairing hint, so we never render hours-old numbers as if they were live.
static void build_idle_group(lv_obj_t* parent) {
    idle_group = lv_obj_create(parent);
    lv_obj_set_size(idle_group, L.scr_w, L.scr_h - L.content_y);
    lv_obj_set_pos(idle_group, 0, L.content_y);
    lv_obj_set_style_bg_opa(idle_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(idle_group, 0, 0);
    lv_obj_set_style_pad_all(idle_group, 0, 0);
    lv_obj_clear_flag(idle_group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(idle_group, LV_OBJ_FLAG_EVENT_BUBBLE);

    // A shrunk-down sleeping creature (reused claudepix "expression sleep" art)
    // sits between the header and the status line; the animated "Listening…"
    // status line carries the words, so no extra text is needed here.
    lv_obj_t* creature = splash_mini_create(idle_group, "expression sleep", 160);
    if (creature) lv_obj_align(creature, LV_ALIGN_CENTER, 0, -20);

    lv_obj_add_flag(idle_group, LV_OBJ_FLAG_HIDDEN);  // update_view_state decides
}

static void init_usage_screen(lv_obj_t* scr) {
    usage_container = lv_obj_create(scr);
    lv_obj_set_size(usage_container, L.scr_w, L.scr_h);
    lv_obj_set_pos(usage_container, 0, 0);
    lv_obj_set_style_bg_opa(usage_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(usage_container, 0, 0);
    lv_obj_set_style_pad_all(usage_container, 0, 0);
    lv_obj_clear_flag(usage_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(usage_container, global_click_cb, LV_EVENT_CLICKED, NULL);

    lbl_title = lv_label_create(usage_container);
    lv_label_set_text(lbl_title, "Usage");
    lv_obj_set_style_text_font(lbl_title, &font_tiempos_56, 0);
    lv_obj_set_style_text_color(lbl_title, COL_TEXT, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 16, L.title_y);

    // Usage panels (shown when connected) live in a transparent full-size group
    // so they can be toggled against the pairing hint as one unit.
    usage_group = lv_obj_create(usage_container);
    lv_obj_set_size(usage_group, L.scr_w, L.scr_h);
    lv_obj_set_pos(usage_group, 0, 0);
    lv_obj_set_style_bg_opa(usage_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(usage_group, 0, 0);
    lv_obj_set_style_pad_all(usage_group, 0, 0);
    lv_obj_clear_flag(usage_group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(usage_group, LV_OBJ_FLAG_EVENT_BUBBLE);

    panel_session = make_usage_panel(usage_group, L.content_y, "Current",
                     &lbl_session_pct, &lbl_session_label,
                     &bar_session, &lbl_session_reset);

    // Enterprise-only overlays inside panel_session — hidden until enterprise data arrives
    lbl_session_pct_sym = lv_label_create(panel_session);
    lv_label_set_text(lbl_session_pct_sym, "%");
    lv_obj_set_style_text_font(lbl_session_pct_sym, &font_styrene_28, 0);
    lv_obj_set_style_text_color(lbl_session_pct_sym, COL_TEXT, 0);
    lv_obj_add_flag(lbl_session_pct_sym, LV_OBJ_FLAG_HIDDEN);

    lbl_spending_desc = lv_label_create(panel_session);
    lv_label_set_text(lbl_spending_desc, "of your monthly budget");
    lv_obj_set_style_text_font(lbl_spending_desc, &font_styrene_28, 0);
    lv_obj_set_style_text_color(lbl_spending_desc, COL_DIM, 0);
    lv_obj_set_pos(lbl_spending_desc, 0, L.usage_reset_y);
    lv_obj_add_flag(lbl_spending_desc, LV_OBJ_FLAG_HIDDEN);

    lbl_spending_status = lv_label_create(panel_session);
    lv_label_set_text(lbl_spending_status, "");
    lv_obj_set_style_text_font(lbl_spending_status, &font_styrene_16, 0);
    lv_obj_set_pos(lbl_spending_status, 0, L.usage_reset_y + 20);
    lv_obj_add_flag(lbl_spending_status, LV_OBJ_FLAG_HIDDEN);

    panel_weekly = make_usage_panel(usage_group,
                     L.content_y + L.usage_panel_h + L.usage_panel_gap, "Weekly",
                     &lbl_weekly_pct, &lbl_weekly_label,
                     &bar_weekly, &lbl_weekly_reset);
    // Recolor enabled so enterprise period box can color pace and reset separately
    lv_label_set_recolor(lbl_weekly_reset, true);

    build_pair_group(usage_container);
    build_idle_group(usage_container);

    // Status line — always visible on the usage view. Driven by ui_tick_anim().
    lbl_anim = lv_label_create(usage_container);
    lv_label_set_text(lbl_anim, "");
    lv_obj_set_style_text_font(lbl_anim, &font_mono_32, 0);
    lv_obj_set_style_text_color(lbl_anim, COL_ACCENT, 0);
    lv_obj_align(lbl_anim, LV_ALIGN_BOTTOM_MID, 0, -15);
}

// ======== Status Screen ========

// WMO weather code → a short condition word. Fonts are ASCII-only, so no icon.
static const char* weather_cond_word(int code) {
    switch (code) {
    case 0:  return "Clear";
    case 1:  return "Mainly clear";
    case 2:  return "Partly cloudy";
    case 3:  return "Cloudy";
    case 45: case 48: return "Fog";
    case 51: case 53: case 55: return "Drizzle";
    case 61: case 63: case 65: return "Rain";
    case 66: case 67: return "Freezing rain";
    case 71: case 73: case 75: case 77: return "Snow";
    case 80: case 81: case 82: return "Showers";
    case 85: case 86: return "Snow showers";
    case 95: case 96: case 99: return "Thunderstorm";
    default: return "--";
    }
}

// weather_dscs[] index order — matches the icons generated in icons_status.h.
enum { WI_SUN, WI_SUNCLOUD, WI_CLOUD, WI_FOG, WI_RAIN, WI_SNOW, WI_THUNDER };

// WMO weather code → weather_dscs[] index.
static int weather_icon_for(int code) {
    switch (code) {
    case 0:  return WI_SUN;
    case 1: case 2: return WI_SUNCLOUD;
    case 3:  return WI_CLOUD;
    case 45: case 48: return WI_FOG;
    case 51: case 53: case 55: case 61: case 63: case 65:
    case 66: case 67: case 80: case 81: case 82: return WI_RAIN;
    case 71: case 73: case 75: case 77: case 85: case 86: return WI_SNOW;
    case 95: case 96: case 99: return WI_THUNDER;
    default: return WI_CLOUD;
    }
}

// Per-agent identity icon data (icons_status.h) — general,etsy,upwork,appdev,heirpaws.
static const uint8_t* const AGENT_ICON_DATA[5] = {
    icon_sv_claude_data, icon_sv_github_data, icon_sv_devops_data,
    icon_sv_shopify_data, icon_sv_minipc_data
};
static const char* const SV_NAMES[5] = {"Claude", "GitHub", "DevOps", "Shopify", "MiniPC"};

static void init_status_screen(lv_obj_t* scr) {
    status_container = lv_obj_create(scr);
    lv_obj_set_size(status_container, L.scr_w, L.scr_h);
    lv_obj_set_pos(status_container, 0, 0);
    lv_obj_set_style_bg_opa(status_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(status_container, 0, 0);
    lv_obj_set_style_pad_all(status_container, 0, 0);
    lv_obj_clear_flag(status_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(status_container, global_click_cb, LV_EVENT_CLICKED, NULL);

    // --- Clock (hero, top-center) — same slot as the usage title, clear of
    //     the top-left logo and top-right battery which auto-show here. ---
    lbl_status_clock = lv_label_create(status_container);
    lv_label_set_text(lbl_status_clock, "--:--");
    lv_obj_set_style_text_font(lbl_status_clock, &font_tiempos_56, 0);
    lv_obj_set_style_text_color(lbl_status_clock, COL_TEXT, 0);
    lv_obj_align(lbl_status_clock, LV_ALIGN_TOP_MID, 16, L.title_y);

    // --- Weather panel (left): [flag] London / temp + condition icon / condition word ---
    lv_obj_t* wpanel = make_panel(status_container, L.margin, L.content_y, 248, 150);

    init_icon_dsc_rgb565a8(&flag_dsc, ICON_FLAG_UK_W, ICON_FLAG_UK_H, icon_flag_uk_data);
    img_flag = lv_image_create(wpanel);
    lv_image_set_src(img_flag, &flag_dsc);
    lv_obj_set_pos(img_flag, 0, 3);
    lv_obj_add_flag(img_flag, LV_OBJ_FLAG_HIDDEN);   // Murcia: sin bandera

    lv_obj_t* wlbl = lv_label_create(wpanel);
    lv_label_set_text(wlbl, "Murcia");
    lv_obj_set_style_text_font(wlbl, &font_styrene_20, 0);
    lv_obj_set_style_text_color(wlbl, COL_DIM, 0);
    lv_obj_set_pos(wlbl, 0, 1);

    lbl_weather_temp = lv_label_create(wpanel);
    lv_label_set_text(lbl_weather_temp, "--");
    lv_obj_set_style_text_font(lbl_weather_temp, &font_styrene_48, 0);
    lv_obj_set_style_text_color(lbl_weather_temp, COL_TEXT, 0);
    lv_obj_set_pos(lbl_weather_temp, 0, 28);

    // Degree mark: a small hollow ring superscripted to the number.
    deg_ring = lv_obj_create(wpanel);
    lv_obj_set_size(deg_ring, 13, 13);
    lv_obj_set_style_radius(deg_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(deg_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(deg_ring, 3, 0);
    lv_obj_set_style_border_color(deg_ring, COL_TEXT, 0);
    lv_obj_set_style_pad_all(deg_ring, 0, 0);
    lv_obj_clear_flag(deg_ring, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(deg_ring, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_align_to(deg_ring, lbl_weather_temp, LV_ALIGN_OUT_RIGHT_TOP, 4, 8);
    lv_obj_add_flag(deg_ring, LV_OBJ_FLAG_HIDDEN);   // revealed by ui_update once a temp arrives

    // Condition icon (top-right), swapped by wc in ui_update.
    init_icon_dsc_rgb565a8(&weather_dscs[WI_SUN],      ICON_WX_SUN_W,      ICON_WX_SUN_H,      icon_wx_sun_data);
    init_icon_dsc_rgb565a8(&weather_dscs[WI_SUNCLOUD], ICON_WX_SUNCLOUD_W, ICON_WX_SUNCLOUD_H, icon_wx_suncloud_data);
    init_icon_dsc_rgb565a8(&weather_dscs[WI_CLOUD],    ICON_WX_CLOUD_W,    ICON_WX_CLOUD_H,    icon_wx_cloud_data);
    init_icon_dsc_rgb565a8(&weather_dscs[WI_FOG],      ICON_WX_FOG_W,      ICON_WX_FOG_H,      icon_wx_fog_data);
    init_icon_dsc_rgb565a8(&weather_dscs[WI_RAIN],     ICON_WX_RAIN_W,     ICON_WX_RAIN_H,     icon_wx_rain_data);
    init_icon_dsc_rgb565a8(&weather_dscs[WI_SNOW],     ICON_WX_SNOW_W,     ICON_WX_SNOW_H,     icon_wx_snow_data);
    init_icon_dsc_rgb565a8(&weather_dscs[WI_THUNDER],  ICON_WX_THUNDER_W,  ICON_WX_THUNDER_H,  icon_wx_thunder_data);
    img_weather = lv_image_create(wpanel);
    lv_image_set_src(img_weather, &weather_dscs[WI_CLOUD]);
    lv_obj_set_pos(img_weather, 168, 24);
    lv_obj_add_flag(img_weather, LV_OBJ_FLAG_HIDDEN);   // revealed by ui_update

    lbl_weather_cond = lv_label_create(wpanel);
    lv_label_set_text(lbl_weather_cond, "--");
    lv_obj_set_style_text_font(lbl_weather_cond, &font_styrene_24, 0);
    lv_obj_set_style_text_color(lbl_weather_cond, COL_ACCENT, 0);
    lv_obj_set_pos(lbl_weather_cond, 0, 92);

    // --- Tomorrow's weather panel (right): Tomorrow / high temp + icon / condition ---
    lv_obj_t* tpanel = make_panel(status_container, L.margin + 248 + 16, L.content_y, 176, 150);

    lv_obj_t* tlbl = lv_label_create(tpanel);
    lv_label_set_text(tlbl, "Tomorrow");
    lv_obj_set_style_text_font(tlbl, &font_styrene_20, 0);
    lv_obj_set_style_text_color(tlbl, COL_DIM, 0);
    lv_obj_set_pos(tlbl, 0, 0);

    lbl_tmr_high = lv_label_create(tpanel);
    lv_label_set_text(lbl_tmr_high, "--");
    lv_obj_set_style_text_font(lbl_tmr_high, &font_styrene_48, 0);
    lv_obj_set_style_text_color(lbl_tmr_high, COL_TEXT, 0);
    lv_obj_set_pos(lbl_tmr_high, 0, 28);

    // Degree mark: small hollow ring superscripted to the number (fonts are ASCII-only).
    deg_ring_tmr = lv_obj_create(tpanel);
    lv_obj_set_size(deg_ring_tmr, 13, 13);
    lv_obj_set_style_radius(deg_ring_tmr, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(deg_ring_tmr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(deg_ring_tmr, 3, 0);
    lv_obj_set_style_border_color(deg_ring_tmr, COL_TEXT, 0);
    lv_obj_set_style_pad_all(deg_ring_tmr, 0, 0);
    lv_obj_clear_flag(deg_ring_tmr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(deg_ring_tmr, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_align_to(deg_ring_tmr, lbl_tmr_high, LV_ALIGN_OUT_RIGHT_TOP, 4, 8);
    lv_obj_add_flag(deg_ring_tmr, LV_OBJ_FLAG_HIDDEN);   // revealed by ui_update

    // Tomorrow condition icon (top-right), reuses weather_dscs[]; swapped by wc2 in ui_update.
    img_tmr = lv_image_create(tpanel);
    lv_image_set_src(img_tmr, &weather_dscs[WI_CLOUD]);
    lv_obj_set_pos(img_tmr, 100, 24);
    lv_obj_add_flag(img_tmr, LV_OBJ_FLAG_HIDDEN);   // revealed by ui_update

    lbl_tmr_cond = lv_label_create(tpanel);
    lv_label_set_text(lbl_tmr_cond, "--");
    lv_obj_set_style_text_font(lbl_tmr_cond, &font_styrene_24, 0);
    lv_obj_set_style_text_color(lbl_tmr_cond, COL_ACCENT, 0);
    lv_obj_set_pos(lbl_tmr_cond, 0, 92);

    // --- Agents panel (full width): identity icon on top, health dot below ---
    lv_obj_t* apanel = make_panel(status_container, L.margin,
                                  L.content_y + 150 + 16, L.content_w, 150);

    lv_obj_t* albl = lv_label_create(apanel);
    lv_label_set_text(albl, "Servicios");
    lv_obj_set_style_text_font(albl, &font_styrene_20, 0);
    lv_obj_set_style_text_color(albl, COL_DIM, 0);
    lv_obj_set_pos(albl, 0, 0);

    // Inner content width after make_panel's 16px side padding.
    const int inner_w = L.content_w - 32;
    for (int i = 0; i < 5; i++) {
        int center_x = inner_w * (2 * i + 1) / 10;   // evenly spaced column centers

        // Orange highlight behind the whole column — pulses when the agent is working.
        // Created before the icon/dot so they render on top of it.
        int hl_w = inner_w / 5 - 8;
        lv_obj_t* hl = lv_obj_create(apanel);
        lv_obj_set_size(hl, hl_w, 100);
        lv_obj_set_pos(hl, center_x - hl_w / 2, 22);
        lv_obj_set_style_radius(hl, 14, 0);
        lv_obj_set_style_bg_color(hl, COL_ACCENT, 0);
        lv_obj_set_style_bg_opa(hl, LV_OPA_TRANSP, 0);   // invisible until busy
        lv_obj_set_style_border_width(hl, 0, 0);
        lv_obj_set_style_pad_all(hl, 0, 0);
        lv_obj_clear_flag(hl, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(hl, LV_OBJ_FLAG_EVENT_BUBBLE);
        agent_hl[i] = hl;

        // Identity icon on top (all agent icons are 30x30).
        init_icon_dsc_rgb565a8(&agent_icon_dscs[i], ICON_AG_GENERAL_W, ICON_AG_GENERAL_H, AGENT_ICON_DATA[i]);
        lv_obj_t* ic = lv_image_create(apanel);
        lv_image_set_src(ic, &agent_icon_dscs[i]);
        lv_obj_set_pos(ic, center_x - 15, 28);
        lv_obj_add_flag(ic, LV_OBJ_FLAG_EVENT_BUBBLE);

        // Health dot below (green=alive, red=down; gray until data arrives).
        lv_obj_t* dot = lv_obj_create(apanel);
        lv_obj_set_size(dot, 26, 26);
        lv_obj_set_pos(dot, center_x - 13, 70);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, COL_DIM, 0);   // gray = unknown until data arrives
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_pad_all(dot, 0, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(dot, LV_OBJ_FLAG_EVENT_BUBBLE);
        agent_dots[i] = dot;

        lv_obj_t* nl = lv_label_create(apanel);
        lv_label_set_text(nl, SV_NAMES[i]);
        lv_obj_set_style_text_font(nl, &font_styrene_16, 0);
        lv_obj_set_style_text_color(nl, COL_DIM, 0);
        lv_obj_set_style_text_align(nl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(nl, inner_w / 5);
        lv_obj_set_pos(nl, center_x - inner_w / 10, 102);
        lv_obj_add_flag(nl, LV_OBJ_FLAG_EVENT_BUBBLE);
    }
}

// ======== Music Screen ========

static void media_btn_cb(lv_event_t* e) {
    uint8_t cmd = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    ble_send_media_cmd(cmd);
    // Flip the glyph right away; the daemon's next update confirms the real state.
    if (cmd == MEDIA_CMD_PLAYPAUSE && s_music.active) {
        s_music.playing = !s_music.playing;
        lv_label_set_text(lbl_music_play, s_music.playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    }
}

// Round transport button. Deliberately NOT event-bubbling: a tap here must not
// reach the container's global_click_cb (which toggles the splash).
static lv_obj_t* make_media_btn(lv_obj_t* parent, int size, bool primary,
                                const char* glyph, const lv_font_t* font, uint8_t cmd,
                                lv_obj_t** out_label) {
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_set_size(btn, size, size);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, primary ? COL_ACCENT : COL_PANEL, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_60, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, media_btn_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)cmd);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, glyph);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
    lv_obj_center(lbl);
    if (out_label) *out_label = lbl;
    return btn;
}

static void format_mmss(int secs, char* buf, size_t len) {
    if (secs < 0) secs = 0;
    snprintf(buf, len, "%d:%02d", secs / 60, secs % 60);
}

static void init_music_screen(lv_obj_t* scr) {
    music_container = lv_obj_create(scr);
    lv_obj_set_size(music_container, L.scr_w, L.scr_h);
    lv_obj_set_pos(music_container, 0, 0);
    lv_obj_set_style_bg_opa(music_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(music_container, 0, 0);
    lv_obj_set_style_pad_all(music_container, 0, 0);
    lv_obj_clear_flag(music_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(music_container, global_click_cb, LV_EVENT_CLICKED, NULL);

    // Cover slot — a rounded panel with a note glyph until the cover arrives.
    music_cover = lv_obj_create(music_container);
    lv_obj_set_size(music_cover, L.music_cover, L.music_cover);
    lv_obj_align(music_cover, LV_ALIGN_TOP_MID, 0, L.music_cover_y);
    lv_obj_set_style_bg_color(music_cover, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(music_cover, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(music_cover, 12, 0);
    lv_obj_set_style_border_width(music_cover, 0, 0);
    lv_obj_set_style_pad_all(music_cover, 0, 0);
    lv_obj_set_style_clip_corner(music_cover, true, 0);
    lv_obj_clear_flag(music_cover, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(music_cover, LV_OBJ_FLAG_EVENT_BUBBLE);
    music_note = lv_label_create(music_cover);
    lv_label_set_text(music_note, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(music_note, L.music_play_font, 0);
    lv_obj_set_style_text_color(music_note, COL_DIM, 0);
    lv_obj_center(music_note);

    // Decoded cover sits on top of the placeholder; the panel's clip_corner rounds it.
#ifdef BOARD_HAS_PSRAM
    cover_px = (uint16_t*)heap_caps_malloc((size_t)L.music_cover * L.music_cover * 2,
                                           MALLOC_CAP_SPIRAM);
#endif
    if (cover_px) {
        cover_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        cover_dsc.header.w = L.music_cover;
        cover_dsc.header.h = L.music_cover;
        cover_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
        cover_dsc.header.stride = L.music_cover * 2;
        cover_dsc.data = (const uint8_t*)cover_px;
        cover_dsc.data_size = (uint32_t)L.music_cover * L.music_cover * 2;
        img_music_cover = lv_image_create(music_cover);
        lv_obj_set_pos(img_music_cover, 0, 0);
        lv_obj_add_flag(img_music_cover, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_flag(img_music_cover, LV_OBJ_FLAG_HIDDEN);
    }

    int y = L.music_cover_y + L.music_cover + 14;

    // Long titles scroll as a marquee instead of wrapping into the controls.
    lbl_music_title = lv_label_create(music_container);
    lv_label_set_long_mode(lbl_music_title, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_width(lbl_music_title, L.content_w);
    lv_obj_set_style_text_align(lbl_music_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl_music_title, L.music_title_font, 0);
    lv_obj_set_style_text_color(lbl_music_title, COL_TEXT, 0);
    lv_obj_set_pos(lbl_music_title, L.margin, y);
    lv_label_set_text(lbl_music_title, "");
    y += lv_font_get_line_height(L.music_title_font) + 6;

    lbl_music_artist = lv_label_create(music_container);
    lv_label_set_long_mode(lbl_music_artist, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_width(lbl_music_artist, L.content_w);
    lv_obj_set_style_text_align(lbl_music_artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl_music_artist, L.music_artist_font, 0);
    lv_obj_set_style_text_color(lbl_music_artist, COL_DIM, 0);
    lv_obj_set_pos(lbl_music_artist, L.margin, y);
    lv_label_set_text(lbl_music_artist, "");
    y += lv_font_get_line_height(L.music_artist_font) + 14;

    const int bar_x = L.margin + 20;
    const int bar_w = L.content_w - 40;
    bar_music = make_bar(music_container, bar_x, y, bar_w, 8);
    lv_bar_set_range(bar_music, 0, 1000);
    lv_obj_set_style_bg_color(bar_music, COL_ACCENT, LV_PART_INDICATOR);
    lv_obj_add_flag(bar_music, LV_OBJ_FLAG_EVENT_BUBBLE);
    y += 8 + 4;

    lbl_music_pos = lv_label_create(music_container);
    lv_obj_set_style_text_font(lbl_music_pos, L.music_time_font, 0);
    lv_obj_set_style_text_color(lbl_music_pos, COL_DIM, 0);
    lv_obj_set_pos(lbl_music_pos, bar_x, y);
    lv_label_set_text(lbl_music_pos, "");

    lbl_music_dur = lv_label_create(music_container);
    lv_obj_set_style_text_font(lbl_music_dur, L.music_time_font, 0);
    lv_obj_set_style_text_color(lbl_music_dur, COL_DIM, 0);
    lv_obj_set_style_text_align(lbl_music_dur, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(lbl_music_dur, 100);
    lv_obj_set_pos(lbl_music_dur, bar_x + bar_w - 100, y);
    lv_label_set_text(lbl_music_dur, "");

    // Transport row, centred under the progress bar, kept clear of the bottom edge.
    const int row_cy = L.scr_h - 24 - L.music_btn_play / 2;
    const int gap = 28;
    const int cx = L.scr_w / 2;
    lv_obj_t* b;
    b = make_media_btn(music_container, L.music_btn, false, LV_SYMBOL_PREV,
                       L.music_icon_font, MEDIA_CMD_PREV, nullptr);
    lv_obj_set_pos(b, cx - L.music_btn_play / 2 - gap - L.music_btn, row_cy - L.music_btn / 2);
    b = make_media_btn(music_container, L.music_btn_play, true, LV_SYMBOL_PLAY,
                       L.music_play_font, MEDIA_CMD_PLAYPAUSE, &lbl_music_play);
    lv_obj_set_pos(b, cx - L.music_btn_play / 2, row_cy - L.music_btn_play / 2);
    b = make_media_btn(music_container, L.music_btn, false, LV_SYMBOL_NEXT,
                       L.music_icon_font, MEDIA_CMD_NEXT, nullptr);
    lv_obj_set_pos(b, cx + L.music_btn_play / 2 + gap, row_cy - L.music_btn / 2);
}

// Show the decoded cover only when it belongs to the current track; otherwise
// fall back to the note glyph (no cover, or the new one hasn't arrived yet).
static void apply_cover(bool active) {
    if (!img_music_cover) return;
    bool show = active && s_music.art_id != 0 && s_music.art_id == cover_id;
    bool shown = !lv_obj_has_flag(img_music_cover, LV_OBJ_FLAG_HIDDEN);
    if (show == shown) return;
    if (show) {
        lv_obj_clear_flag(img_music_cover, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(music_note, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(img_music_cover, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(music_note, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_set_music_art(uint16_t id, const uint8_t* jpg, uint32_t len) {
    if (!cover_px || id == 0) return;
    if (!cover_decode(jpg, len, cover_px, L.music_cover, L.music_cover)) return;
    cover_id = id;
    // Same descriptor, new pixels. RGB565 variable images aren't cached (LVGL's
    // image cache is off by default), so re-pointing + invalidating is enough.
    lv_image_set_src(img_music_cover, &cover_dsc);
    lv_obj_invalidate(img_music_cover);
    lv_obj_add_flag(img_music_cover, LV_OBJ_FLAG_HIDDEN);   // let apply_cover decide
    apply_cover(music_shown_active == 1);
}

// Redraw the music screen from s_music. "Nothing playing" when the host has no
// media session, the link is down, or the daemon's updates have gone stale.
static void draw_music(uint32_t now) {
    bool active = s_music.active && s_ble_connected &&
                  (now - music_rx_ms) < MUSIC_FRESH_MS;

    if ((int)active != music_shown_active) {
        music_shown_active = active;
        if (!active) {
            lv_label_set_text(lbl_music_title, "Nada sonando");
            lv_label_set_text(lbl_music_artist, s_ble_connected ? "Abre Spotify en el PC" : "Sin conexion");
            lv_label_set_text(lbl_music_pos, "");
            lv_label_set_text(lbl_music_dur, "");
            lv_bar_set_value(bar_music, 0, LV_ANIM_OFF);
            lv_label_set_text(lbl_music_play, LV_SYMBOL_PLAY);
        }
    }
    apply_cover(active);
    if (!active) return;

    int pos = s_music.position;
    if (s_music.playing) pos += (int)((now - music_rx_ms) / 1000);
    if (s_music.duration > 0 && pos > s_music.duration) pos = s_music.duration;

    char buf[16];
    format_mmss(pos, buf, sizeof(buf));
    lv_label_set_text(lbl_music_pos, buf);
    if (s_music.duration > 0) {
        format_mmss(s_music.duration, buf, sizeof(buf));
        lv_label_set_text(lbl_music_dur, buf);
        lv_bar_set_value(bar_music, pos * 1000 / s_music.duration, LV_ANIM_OFF);
    } else {
        lv_label_set_text(lbl_music_dur, "");
        lv_bar_set_value(bar_music, 0, LV_ANIM_OFF);
    }
}

void ui_update_music(const MusicData* data) {
    bool text_changed = strcmp(data->title, s_music.title) != 0 ||
                        strcmp(data->artist, s_music.artist) != 0;
    s_music = *data;
    music_rx_ms = lv_tick_get();
    if (!lbl_music_title) return;

    // Only touch the scrolling labels when the text really changes — re-setting
    // the same text restarts the marquee from the beginning.
    if (s_music.active && (text_changed || music_shown_active != 1)) {
        lv_label_set_text(lbl_music_title, s_music.title);
        lv_label_set_text(lbl_music_artist, s_music.artist);
    }
    if (s_music.active) {
        lv_label_set_text(lbl_music_play, s_music.playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
        music_shown_active = 1;
    }
    draw_music(music_rx_ms);
    music_last_draw_ms = music_rx_ms;
}

// ======== Public API ========

void ui_init(void) {
    compute_layout(board_caps());

    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    init_icon_dsc_rgb565a8(&logo_dsc, LOGO_WIDTH, LOGO_HEIGHT, logo_data);
    init_battery_icons();

    init_usage_screen(scr);
    init_status_screen(scr);
    init_music_screen(scr);
    splash_init(scr);

    if (splash_get_root()) {
        lv_obj_add_event_cb(splash_get_root(), global_click_cb, LV_EVENT_CLICKED, NULL);
    }

    // Animated Clawd logo (top-left, both pages): a subtle blinking claudepix
    // creature. Falls back to the static logo image if the sprite can't allocate
    // (e.g. tight internal SRAM on a PSRAM-free board).
    logo_img = splash_sprite_create(&logo_sprite, scr, "idle blink", 80);
    if (!logo_img) {
        logo_img = lv_image_create(scr);
        lv_image_set_src(logo_img, &logo_dsc);
    }
    lv_obj_set_pos(logo_img, L.margin, L.title_y - 10);

    battery_img = lv_image_create(scr);
    lv_image_set_src(battery_img, &battery_dscs[0]);
    lv_obj_set_pos(battery_img, L.scr_w - 48 - L.margin, L.title_y);

    lbl_batt_pct = lv_label_create(scr);
    lv_label_set_text(lbl_batt_pct, "");
    lv_obj_set_style_text_font(lbl_batt_pct, &font_styrene_20, 0);
    lv_obj_set_style_text_color(lbl_batt_pct, COL_DIM, 0);
    lv_obj_set_style_text_align(lbl_batt_pct, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(lbl_batt_pct, 70);
    lv_obj_align_to(lbl_batt_pct, battery_img, LV_ALIGN_OUT_LEFT_MID, -2, 0);

}

void ui_update(const UsageData* data) {
    if (!data->valid) return;
    last_data_ms = lv_tick_get();   // a valid usage update just landed → dot goes green
    data_received = true;

    if (data->clock_epoch > 0) {    // daemon supplied wall-clock time → drive the title clock
        clock_base_epoch = data->clock_epoch;
        clock_base_ms = last_data_ms;
        clock_fmt = data->clock_fmt;
    } else if (clock_base_epoch != 0) {   // clock turned off daemon-side → revert title to "Usage"
        clock_base_epoch = 0;
        clock_last_min = -1;
        lv_label_set_text(lbl_title, "Usage");
    }

    int s_pct = (int)(data->session_pct + 0.5f);

    if (data->enterprise) {
        // Spending box: big number-only label + small "%" symbol + desc + pace
        lv_obj_set_style_text_font(lbl_session_pct, &font_tiempos_56, 0);
        lv_label_set_text(lbl_session_label, "Spending");
        lv_obj_add_flag(lbl_session_reset, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lbl_session_pct_sym, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lbl_spending_desc,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_spending_status,   LV_OBJ_FLAG_HIDDEN);
        if (panel_weekly) lv_obj_clear_flag(panel_weekly, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_style_text_font(lbl_session_pct, &font_styrene_48, 0);
        lv_label_set_text(lbl_session_label, "Current");
        lv_obj_clear_flag(lbl_session_reset, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_session_pct_sym, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_spending_desc,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_spending_status, LV_OBJ_FLAG_HIDDEN);
        if (panel_weekly) lv_obj_clear_flag(panel_weekly, LV_OBJ_FLAG_HIDDEN);
    }

    char buf[48];

    // Pace vars used in both enterprise blocks below
    const char* pace_text = "Under pace";
    lv_color_t  pace_color = COL_GREEN;
    const char* pace_hex   = "788c5d";   // matches THEME_GREEN
    if (data->session_pct > (float)data->time_pct + 15.0f) {
        pace_text = "Over pace";  pace_color = COL_RED;   pace_hex = "c0392b";
    } else if (data->session_pct > (float)data->time_pct - 15.0f) {
        pace_text = "On pace";    pace_color = COL_AMBER; pace_hex = "d97757";
    }

    if (data->enterprise) {
        lv_label_set_text_fmt(lbl_session_pct, "%d", s_pct);
        lv_obj_align_to(lbl_session_pct_sym, lbl_session_pct,
                        LV_ALIGN_OUT_RIGHT_TOP, 4, 12);
    } else {
        lv_label_set_text_fmt(lbl_session_pct, "%d%%", s_pct);
        format_reset_time(data->session_reset_mins, buf, sizeof(buf));
        lv_label_set_text(lbl_session_reset, buf);
    }

    lv_bar_set_value(bar_session, s_pct, LV_ANIM_ON);
    lv_obj_set_style_bg_color(bar_session, pct_color(data->session_pct), LV_PART_INDICATOR);

    if (data->enterprise) {
        // Period box: time % + dynamic pace color + "Resets <date>" label
        lv_label_set_text(lbl_weekly_label, "Period");
        lv_label_set_text_fmt(lbl_weekly_pct, "%d%%", data->time_pct);
        lv_bar_set_value(bar_weekly, data->time_pct, LV_ANIM_ON);
        lv_color_t bar_pace = (data->session_pct <= (float)data->time_pct) ? COL_GREEN :
                              (data->session_pct <= (float)data->time_pct + 15.0f) ? COL_AMBER :
                              COL_RED;
        lv_obj_set_style_bg_color(bar_weekly, bar_pace, LV_PART_INDICATOR);
        snprintf(buf, sizeof(buf), "#%s %s# - #faf9f5 Resets %s#",
                 pace_hex, pace_text, data->reset_date);
        lv_label_set_text(lbl_weekly_reset, buf);
    } else {
        int w_pct = (int)(data->weekly_pct + 0.5f);
        lv_label_set_text_fmt(lbl_weekly_pct, "%d%%", w_pct);
        lv_bar_set_value(bar_weekly, w_pct, LV_ANIM_ON);
        lv_obj_set_style_bg_color(bar_weekly, pct_color(data->weekly_pct), LV_PART_INDICATOR);
        format_reset_time(data->weekly_reset_mins, buf, sizeof(buf));
        lv_label_set_text(lbl_weekly_reset, buf);
    }

    // ---- Status-screen widgets (persist regardless of which screen is shown) ----
    if (data->weather_temp > -900) {
        lv_label_set_text_fmt(lbl_weather_temp, "%d", data->weather_temp);
        lv_obj_align_to(deg_ring, lbl_weather_temp, LV_ALIGN_OUT_RIGHT_TOP, 4, 8);
        lv_obj_clear_flag(deg_ring, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl_weather_cond, weather_cond_word(data->weather_cond));
        lv_image_set_src(img_weather, &weather_dscs[weather_icon_for(data->weather_cond)]);
        lv_obj_clear_flag(img_weather, LV_OBJ_FLAG_HIDDEN);
    }
    if (data->agents_present) {
        for (int i = 0; i < 5; i++) {
            lv_obj_set_style_bg_color(agent_dots[i],
                                      data->agent_unknown[i] ? COL_DIM
                                      : (data->agents[i] ? COL_GREEN : COL_RED), 0);
            // Orange highlight pulses on agents that are actively working (see tick loop).
            s_agent_busy[i] = data->agent_busy[i];
            if (agent_hl[i] && !data->agent_busy[i])
                lv_obj_set_style_bg_opa(agent_hl[i], LV_OPA_TRANSP, 0);   // clear when idle
        }
    }
    if (data->tomorrow_high > -900) {
        lv_label_set_text_fmt(lbl_tmr_high, "%d", data->tomorrow_high);
        lv_obj_align_to(deg_ring_tmr, lbl_tmr_high, LV_ALIGN_OUT_RIGHT_TOP, 4, 8);
        lv_obj_clear_flag(deg_ring_tmr, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl_tmr_cond, weather_cond_word(data->tomorrow_cond));
        lv_image_set_src(img_tmr, &weather_dscs[weather_icon_for(data->tomorrow_cond)]);
        lv_obj_clear_flag(img_tmr, LV_OBJ_FLAG_HIDDEN);
    }
}

// Pick the usage-view sub-screen: pairing hint (BLE down), the idle "Zzz" screen
// (connected but data has gone stale), or the live usage panels. Only re-lays-out
// on an actual change. The animated status line stays visible everywhere — it
// reads "Listening…" on the idle screen, keeping it alive rather than frozen.
static void update_view_state(void) {
    if (!usage_group || !pair_group || !idle_group) return;
    int v;
    if (!s_ble_connected) {
        v = 0;  // pairing hint
    } else if (data_received && (lv_tick_get() - last_data_ms) < DATA_FRESH_MS) {
        v = 2;  // live usage
    } else {
        v = 1;  // idle / Zzz
    }
    if (v == view_state) return;
    view_state = v;
    lv_obj_add_flag(pair_group, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(idle_group, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(usage_group, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(v == 0 ? pair_group : v == 1 ? idle_group : usage_group,
                      LV_OBJ_FLAG_HIDDEN);
}

// Format the daemon clock as HH:MM (24h) or "H:MM AM/PM" (12h).
static void format_hhmm(const struct tm* t, char* buf, size_t len) {
    if (clock_fmt == 12) {
        int h12 = t->tm_hour % 12;
        if (h12 == 0) h12 = 12;
        snprintf(buf, len, "%d:%02d %s", h12, t->tm_min, t->tm_hour < 12 ? "AM" : "PM");
    } else {
        snprintf(buf, len, "%02d:%02d", t->tm_hour, t->tm_min);
    }
}

// Advance the daemon-supplied wall-clock locally and push HH:MM to both the
// usage-screen title and the status-screen clock. Runs every tick regardless of
// which screen is visible, so the status clock keeps time too. Only rewrites the
// labels when the minute changes.
static void update_clock_labels(uint32_t now) {
    if (clock_base_epoch <= 0) return;   // daemon hasn't sent time (or turned it off)
    time_t cur = (time_t)(clock_base_epoch + (now - clock_base_ms) / 1000);
    struct tm tmv;
    gmtime_r(&cur, &tmv);   // epoch is already local wall-clock → gmtime keeps it as-is
    if (tmv.tm_min == clock_last_min) return;
    clock_last_min = tmv.tm_min;
    char tbuf[16];
    format_hhmm(&tmv, tbuf, sizeof(tbuf));
    lv_label_set_text(lbl_title, tbuf);
    if (lbl_status_clock) lv_label_set_text(lbl_status_clock, tbuf);
}

// Same clock the title/status HH:MM uses: daemon base epoch advanced by the
// local tick delta, read out as local hour/minute. False until a clock lands.
bool ui_get_local_time(int* hour, int* min) {
    if (clock_base_epoch <= 0) return false;
    time_t cur = (time_t)(clock_base_epoch + (lv_tick_get() - clock_base_ms) / 1000);
    struct tm tmv;
    gmtime_r(&cur, &tmv);   // epoch is already local wall-clock → gmtime keeps it as-is
    if (hour) *hour = tmv.tm_hour;
    if (min)  *min  = tmv.tm_min;
    return true;
}

void ui_tick_anim(void) {
    uint32_t now = lv_tick_get();
    update_clock_labels(now);   // both screens' clocks tick, whichever is shown
    if (current_screen != SCREEN_SPLASH) {
        splash_sprite_tick(&logo_sprite);  // animate logo on data pages
        // Pulse the orange highlight on each working agent (triangle wave ~1.4s).
        uint32_t ph = lv_tick_get() % 1400;
        lv_opa_t pulse = (lv_opa_t)(ph < 700 ? 60 + ph * 100 / 700
                                             : 60 + (1400 - ph) * 100 / 700);
        for (int i = 0; i < 5; i++)
            if (agent_hl[i] && s_agent_busy[i]) lv_obj_set_style_bg_opa(agent_hl[i], pulse, 0);
    }

    // Music progress ticks locally between daemon updates; redraw once a second.
    if (current_screen == SCREEN_MUSIC && now - music_last_draw_ms >= 1000) {
        music_last_draw_ms = now;
        draw_music(now);
    }

    if (current_screen != SCREEN_USAGE) return;
    update_view_state();
    if (view_state == 1) splash_mini_tick();   // animate the sleeping creature on the idle screen

    if (now - anim_msg_start >= ANIM_MSG_MS) {
        anim_msg_idx = (anim_msg_idx + 1) % ANIM_MSG_COUNT;
        anim_msg_start = now;
    }

    if (now - anim_last_ms < spinner_ms[anim_spinner_idx]) return;
    anim_last_ms = now;
    anim_phase = (anim_phase + 1) % SPINNER_PHASES;
    anim_spinner_idx = (anim_phase < SPINNER_COUNT) ? anim_phase
                                                    : (SPINNER_PHASES - anim_phase);

    // Status text by priority. Whimsical messages only when connected & settled.
    const char* text;
    if (!s_ble_connected) {
        text = "Waiting";              // advertising / waiting for a host connection
    } else if (view_state == 1) {      // idle — alternate so it reads as alive AND data-less
        text = (anim_msg_idx & 1) ? "No data" : "Listening";
    } else if (now - connected_at_ms < 5000) {
        text = "Connected";
    } else {
        text = anim_messages[anim_msg_idx];
    }

    // All states share the whimsical style: "<glyph> <Title-case word>…"
    static char buf[80];
    snprintf(buf, sizeof(buf), "%s %s\xE2\x80\xA6",
             spinner_frames[anim_spinner_idx], text);
    lv_label_set_text(lbl_anim, buf);
}

static screen_t prev_non_splash_screen = SCREEN_USAGE;
static void apply_battery_visibility(void) {
    if (!battery_img) return;
    if (current_screen == SCREEN_SPLASH) lv_obj_add_flag(battery_img, LV_OBJ_FLAG_HIDDEN);
    else                                  lv_obj_clear_flag(battery_img, LV_OBJ_FLAG_HIDDEN);
    if (lbl_batt_pct) {
        if (current_screen == SCREEN_SPLASH) lv_obj_add_flag(lbl_batt_pct, LV_OBJ_FLAG_HIDDEN);
        else                                  lv_obj_clear_flag(lbl_batt_pct, LV_OBJ_FLAG_HIDDEN);
    }
}

static void global_click_cb(lv_event_t* e) {
    (void)e;
    if (current_screen == SCREEN_SPLASH) ui_show_screen(prev_non_splash_screen);
    else                                  ui_show_screen(SCREEN_SPLASH);
}

void ui_show_screen(screen_t screen) {
    lv_obj_add_flag(usage_container, LV_OBJ_FLAG_HIDDEN);
    if (status_container) lv_obj_add_flag(status_container, LV_OBJ_FLAG_HIDDEN);
    if (music_container) lv_obj_add_flag(music_container, LV_OBJ_FLAG_HIDDEN);
    splash_hide();

    switch (screen) {
    case SCREEN_SPLASH:  splash_show(); break;
    case SCREEN_USAGE:   lv_obj_clear_flag(usage_container, LV_OBJ_FLAG_HIDDEN); break;
    case SCREEN_STATUS:  if (status_container) lv_obj_clear_flag(status_container, LV_OBJ_FLAG_HIDDEN); break;
    case SCREEN_MUSIC:
        if (music_container) lv_obj_clear_flag(music_container, LV_OBJ_FLAG_HIDDEN);
        draw_music(lv_tick_get());
        break;
    default: break;
    }

    if (logo_img) {
        if (screen == SCREEN_SPLASH) lv_obj_add_flag(logo_img, LV_OBJ_FLAG_HIDDEN);
        else                          lv_obj_clear_flag(logo_img, LV_OBJ_FLAG_HIDDEN);
    }

    if (screen != SCREEN_SPLASH) prev_non_splash_screen = screen;
    current_screen = screen;
    apply_battery_visibility();
}

// Cycle the non-splash pages (usage -> status -> music -> usage). Called by the
// PWR button short-press when not on the splash screen.
void ui_cycle_page(void) {
    if      (current_screen == SCREEN_USAGE)  ui_show_screen(SCREEN_STATUS);
    else if (current_screen == SCREEN_STATUS) ui_show_screen(SCREEN_MUSIC);
    else                                      ui_show_screen(SCREEN_USAGE);
}

void ui_toggle_splash(void) {
    if (current_screen == SCREEN_SPLASH) ui_show_screen(prev_non_splash_screen);
    else                                  ui_show_screen(SCREEN_SPLASH);
}

screen_t ui_get_current_screen(void) {
    return current_screen;
}

void ui_update_ble_status(ble_state_t state, const char* name, const char* mac) {
    (void)name; (void)mac;
    bool was_connected = s_ble_connected;
    s_ble_connected = (state == BLE_STATE_CONNECTED);

    if (s_ble_connected && !was_connected) connected_at_ms = lv_tick_get();
    // pair / idle / usage — picked from connection + data freshness.
    update_view_state();
}

void ui_update_battery(int percent, bool charging) {
    int idx;
    if (charging) {
        idx = 4;
    } else if (percent < 0) {
        idx = 0;
    } else if (percent <= 10) {
        idx = 0;
    } else if (percent <= 35) {
        idx = 1;
    } else if (percent <= 75) {
        idx = 2;
    } else {
        idx = 3;
    }
    lv_image_set_src(battery_img, &battery_dscs[idx]);
    if (lbl_batt_pct) {
        if (percent < 0) lv_label_set_text(lbl_batt_pct, "");
        else             lv_label_set_text_fmt(lbl_batt_pct, "%d%%", percent);
    }
    apply_battery_visibility();
}
