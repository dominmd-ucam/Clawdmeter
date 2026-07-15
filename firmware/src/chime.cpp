#include "chime.h"
#include <Arduino.h>
#include "ESP_I2S.h"
#include "es8311.h"
#include "bell_pcm.h"   // const uint8_t bell_pcm[] / bell_pcm_len — 44.1 kHz 16-bit stereo

// Shared ES8311 chime engine. See chime.h. Adapted from the original 2.16
// sound.cpp so the 2.16, 1.8 (and any future ES8311 board) share one copy of
// the codec setup, the embedded PCM, and the non-blocking playback task.

static I2SClass      i2s;
static ChimeConfig   cfg;
static bool          ready   = false;
static volatile bool playing = false;
static es8311_handle_t es_handle = nullptr;   // codec handle (kept alive for the codec's lifetime)

// One-shot voice job handed to voice_task via file-scope statics (only one
// playback runs at a time — serialized by `playing`).
static const uint8_t* job_pcm  = nullptr;
static size_t         job_len  = 0;
static int            job_rate = 0;

// Stereo expansion scratch (mono sample -> L+R). File-scope so it doesn't eat
// the playback task's stack. 256 frames * 2 ch * 2 bytes = 1 KB.
#define VOICE_CHUNK_FRAMES 256
static int16_t voice_stereo[VOICE_CHUNK_FRAMES * 2];

static bool es8311_setup(void) {
    es_handle = es8311_create(0, cfg.es8311_addr);   // I2C port 0 (shared Wire bus)
    if (!es_handle) return false;
    // mclk_inverted, sclk_inverted, mclk_from_mclk_pin, mclk_frequency, sample_frequency
    const es8311_clock_config_t clk = {
        false, false, true, cfg.sample_rate * 256, cfg.sample_rate
    };
    if (es8311_init(es_handle, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) return false;
    es8311_sample_frequency_config(es_handle, clk.mclk_frequency, clk.sample_frequency);
    es8311_microphone_config(es_handle, false);
    es8311_voice_volume_set(es_handle, cfg.volume, NULL);
    return true;
}

static void chime_task(void* arg) {
    if (cfg.amp_enable) cfg.amp_enable(true);
    delay(8);                                  // let the amp settle (avoids turn-on pop)
    i2s.write((uint8_t*)bell_pcm, bell_pcm_len);
    delay(20);
    if (cfg.amp_enable) cfg.amp_enable(false);
    playing = false;
    vTaskDelete(nullptr);
}

// Plays job_pcm (mono, 16-bit LE, job_rate) through the engine's NATIVE rate.
// We deliberately do NOT re-clock the I2S/codec at runtime — that was unreliable
// and garbled the clip. The engine stays at cfg.sample_rate; the source is
// nearest-neighbor upsampled on the fly, then each sample duplicated to L+R.
static void voice_task(void* arg) {
    if (cfg.amp_enable) cfg.amp_enable(true);
    delay(8);                                  // amp settle (avoids turn-on pop)

    const int16_t* src = (const int16_t*)job_pcm;
    size_t   nsamples = job_len / 2;                 // source 16-bit mono samples
    uint32_t in_rate  = (uint32_t)job_rate;          // e.g. 12000
    uint32_t out_rate = (uint32_t)cfg.sample_rate;   // e.g. 44100
    if (in_rate == 0) in_rate = out_rate;
    uint64_t total_out = (uint64_t)nsamples * out_rate / in_rate;

    size_t buf_i = 0;
    for (uint64_t of = 0; of < total_out; of++) {
        size_t si = (size_t)(of * in_rate / out_rate);
        if (si >= nsamples) si = nsamples - 1;
        int16_t s = src[si];
        voice_stereo[buf_i * 2]     = s;       // L
        voice_stereo[buf_i * 2 + 1] = s;       // R
        if (++buf_i == VOICE_CHUNK_FRAMES) {
            i2s.write((uint8_t*)voice_stereo, buf_i * 4);
            buf_i = 0;
        }
    }
    if (buf_i) i2s.write((uint8_t*)voice_stereo, buf_i * 4);

    delay(20);
    if (cfg.amp_enable) cfg.amp_enable(false);
    playing = false;
    vTaskDelete(nullptr);
}

bool chime_init(const ChimeConfig& c) {
    cfg = c;
    if (cfg.amp_enable) cfg.amp_enable(false);   // amp off until we play

    i2s.setPins(cfg.bclk, cfg.ws, cfg.dout, cfg.din, cfg.mclk);
    if (!i2s.begin(I2S_MODE_STD, cfg.sample_rate, I2S_DATA_BIT_WIDTH_16BIT,
                   I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
        Serial.println("chime: I2S init failed");
        return false;
    }
    if (!es8311_setup()) {
        Serial.println("chime: ES8311 init failed");
        return false;
    }
    ready = true;
    Serial.println("chime: ES8311 ready");
    return true;
}

void chime_play(void) {
    if (!ready || playing) return;
    playing = true;
    if (xTaskCreatePinnedToCore(chime_task, "chime", 4096, nullptr, 1, nullptr, 0) != pdPASS)
        playing = false;   // couldn't spawn — stay silent rather than wedge the flag
}

void chime_play_pcm(const uint8_t* pcm, size_t len, int sample_rate) {
    if (!ready || playing) return;
    if (!pcm || len < 2 || sample_rate <= 0) return;
    playing = true;
    job_pcm  = pcm;
    job_len  = len;
    job_rate = sample_rate;
    // Bigger stack than the bell task: the mono->stereo loop + I2S retune run here.
    if (xTaskCreatePinnedToCore(voice_task, "voice", 6144, nullptr, 1, nullptr, 0) != pdPASS)
        playing = false;   // couldn't spawn — stay silent rather than wedge the flag
}

void chime_tick(void) {}   // playback runs in chime_task; nothing to poll
