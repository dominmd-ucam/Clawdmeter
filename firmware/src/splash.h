#pragma once
#include <stdint.h>
#include <lvgl.h>

// Initialize splash module. Creates the canvas widget inside `parent` and
// allocates the 480x480 pixel buffer (PSRAM).
void splash_init(lv_obj_t *parent);

// Advance animation frame if hold time elapsed. Call from main loop.
void splash_tick(void);

// Cycle to the next animation in the catalog.
void splash_next(void);

// Show/hide the splash container.
void splash_show(void);
void splash_hide(void);

// Pick the next animation matching the current usage-rate group.
// Called automatically by splash_show(); also exposed so other modules can
// trigger a re-pick when the rate group changes mid-display.
void splash_pick_for_current_rate(void);

// True when splash is currently rendering (used to gate re-picks).
bool splash_is_active(void);

// Root container (so ui.cpp can attach a click event).
lv_obj_t* splash_get_root(void);

// ---- Embeddable animated sprite (claudepix creature) ----
// Instance-based so multiple animated creatures can coexist (e.g. the top-left
// logo AND the idle-screen sleeper). Each sprite owns its canvas + pixel buffer.
typedef struct {
    lv_obj_t   *canvas;
    uint16_t   *buf;
    int         cell;
    int         w;
    const void *anim;    // splash_anim_def_t* (opaque here)
    uint16_t    frame;
    uint32_t    started;
} splash_sprite_t;

// Render the named animation (e.g. "idle blink") at ~px×px inside `parent`.
// Returns the canvas object (position it with lv_obj_set_pos/align) or NULL if
// the animation isn't found / allocation fails. Drive it with splash_sprite_tick().
lv_obj_t* splash_sprite_create(splash_sprite_t *s, lv_obj_t *parent, const char *anim_name, int px);
void      splash_sprite_tick(splash_sprite_t *s);

// Legacy single-creature helpers (used by the idle screen) — thin wrappers over
// a private static sprite.
lv_obj_t* splash_mini_create(lv_obj_t *parent, const char *anim_name, int px);
void splash_mini_tick(void);
