#include "../../hal/sound_hal.h"
#include "board.h"

#if BOARD_HAS_SOUND

#include <Arduino.h>
#include "../../chime.h"
#include "../../voice_morning_pcm.h"   // 12 kHz mono voice clips (const flash buffers)
#include "../../voice_evening_pcm.h"

// AMOLED-2.16: ES8311 codec + speaker. The power amp is a plain GPIO. All the
// codec/I2S/playback work lives in the shared chime engine (../../chime.cpp);
// this file only supplies the board's pins, the amp-enable hook, and the
// embedded voice clips played on the daily schedule.

#define VOICE_SAMPLE_RATE 12000   // the embedded voice PCM's rate (Hz)

static void amp_enable(bool on) {
    digitalWrite(SND_PA_PIN, on ? HIGH : LOW);
}

void sound_hal_init(void) {
    pinMode(SND_PA_PIN, OUTPUT);
    const ChimeConfig cfg = {
        SND_I2S_MCLK, SND_I2S_BCLK, SND_I2S_WS, SND_I2S_DOUT, SND_I2S_DIN,
        SND_SAMPLE_RATE, SND_ES8311_ADDR, 70, amp_enable
    };
    chime_init(cfg);
}

void sound_hal_play_reset(void) { chime_play(); }
void sound_hal_tick(void)       { chime_tick(); }

void sound_hal_play_voice_morning(void) {
    chime_play_pcm(voice_morning_pcm, voice_morning_pcm_len, VOICE_SAMPLE_RATE);
}
void sound_hal_play_voice_evening(void) {
    chime_play_pcm(voice_evening_pcm, voice_evening_pcm_len, VOICE_SAMPLE_RATE);
}

#endif  // BOARD_HAS_SOUND
