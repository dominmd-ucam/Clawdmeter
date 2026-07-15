#pragma once
#include <stdint.h>
#include <stddef.h>

// Board-agnostic ES8311 + I2S chime engine. A board's sound.cpp fills in a
// ChimeConfig (its I2S pins, codec address, volume, and a power-amp enable
// hook) and calls chime_init() once; chime_play() then streams the embedded
// reset clip (bell_pcm.h) to the codec in a one-shot FreeRTOS task so the LVGL
// loop never blocks. The embedded PCM is 44.1 kHz / 16-bit / stereo, so
// cfg.sample_rate must be 44100 for correct-pitch playback.
//
// The engine can also play arbitrary MONO 16-bit clips at a different sample
// rate via chime_play_pcm() — it retunes the I2S clock + ES8311 to the clip's
// rate, duplicates each mono sample to L+R, then restores cfg.sample_rate. This
// is used for the scheduled 12 kHz voice clips (see voice_*_pcm.h).
//
// amp_enable is the only part that differs structurally between boards: the
// 2.16 toggles a GPIO, the 1.8 drives an XCA9554 expander line. May be null.

typedef void (*amp_enable_fn)(bool on);

struct ChimeConfig {
    int          mclk, bclk, ws, dout, din;  // I2S GPIOs
    int          sample_rate;                // must match the embedded PCM (44100)
    uint8_t      es8311_addr;                // codec I2C address (0x18)
    uint8_t      volume;                     // 0..100
    amp_enable_fn amp_enable;                // power-amp on/off hook (nullable)
};

// Bring up I2S + the ES8311 codec. Returns true on success; false leaves the
// engine inert (chime_play becomes a no-op). Wire/I2C must already be up.
bool chime_init(const ChimeConfig& cfg);

// Queue one playback of the embedded clip (non-blocking). No-op if not ready
// or already playing.
void chime_play(void);

// Queue one playback of an arbitrary MONO, 16-bit little-endian PCM buffer at
// `sample_rate` Hz (non-blocking, own task). The engine retunes the I2S clock
// and the ES8311 to sample_rate, writes each mono sample to both L+R of the
// stereo I2S slot, guards the amp on/off around playback, then restores the
// bell's 44.1 kHz rate. `pcm` must stay valid for the clip's duration (embedded
// const flash buffers do). No-op if not ready or already playing.
void chime_play_pcm(const uint8_t* pcm, size_t len, int sample_rate);

// Currently a no-op (playback runs in its own task); kept for HAL symmetry.
void chime_tick(void);
