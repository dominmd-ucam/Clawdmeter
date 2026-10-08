#pragma once
#include <stdint.h>

// Decode a baseline JPEG (the daemon's cover art) into an RGB565 buffer of
// out_w x out_h pixels. The image is centred: a larger JPEG is cropped, a
// smaller one leaves a black border. Returns false on a corrupt/unsupported file.
bool cover_decode(const uint8_t* jpg, uint32_t len, uint16_t* out, int out_w, int out_h);
