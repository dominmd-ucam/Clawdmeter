#include "cover.h"
#include <Arduino.h>
#include <lvgl.h>
#include <string.h>
#include "src/libs/tjpgd/tjpgd.h"   // TJpgDec bundled with LVGL (LV_USE_TJPGD=1)

// TJpgDec needs ~3.5 KB of work memory at JD_FASTDECODE=1; keep some headroom.
#define COVER_POOL_SIZE 4096

struct CoverCtx {
    const uint8_t* jpg;
    uint32_t len;
    uint32_t pos;
    uint16_t* out;
    int out_w, out_h;
    int off_x, off_y;    // where JPEG pixel (0,0) lands in the output (may be negative)
};

static size_t in_func(JDEC* jd, uint8_t* buf, size_t n) {
    CoverCtx* c = (CoverCtx*)jd->device;
    size_t left = c->len - c->pos;
    if (n > left) n = left;
    if (buf) memcpy(buf, c->jpg + c->pos, n);   // buf == NULL means "skip n bytes"
    c->pos += n;
    return n;
}

// TJpgDec hands over one MCU block at a time as RGB888 (JD_FORMAT 0).
static int out_func(JDEC* jd, void* bitmap, JRECT* r) {
    CoverCtx* c = (CoverCtx*)jd->device;
    const uint8_t* src = (const uint8_t*)bitmap;
    for (int y = r->top; y <= r->bottom; y++) {
        int oy = y + c->off_y;
        for (int x = r->left; x <= r->right; x++, src += 3) {
            int ox = x + c->off_x;
            if (oy < 0 || oy >= c->out_h || ox < 0 || ox >= c->out_w) continue;
            c->out[oy * c->out_w + ox] =
                ((src[0] & 0xF8) << 8) | ((src[1] & 0xFC) << 3) | (src[2] >> 3);
        }
    }
    return 1;
}

bool cover_decode(const uint8_t* jpg, uint32_t len, uint16_t* out, int out_w, int out_h) {
    static uint8_t* pool = nullptr;
    if (!pool) pool = (uint8_t*)malloc(COVER_POOL_SIZE);
    if (!pool) return false;

    CoverCtx c = {jpg, len, 0, out, out_w, out_h, 0, 0};
    JDEC jd;
    JRESULT res = jd_prepare(&jd, in_func, pool, COVER_POOL_SIZE, &c);
    if (res != JDR_OK) {
        Serial.printf("cover: jd_prepare failed (%d)\n", res);
        return false;
    }
    c.off_x = (out_w - (int)jd.width) / 2;
    c.off_y = (out_h - (int)jd.height) / 2;
    memset(out, 0, (size_t)out_w * out_h * 2);
    uint32_t t0 = millis();
    res = jd_decomp(&jd, out_func, 0);
    if (res != JDR_OK) {
        Serial.printf("cover: jd_decomp failed (%d)\n", res);
        return false;
    }
    Serial.printf("cover: %ux%u JPEG (%lu B) decoded in %lu ms\n",
                  jd.width, jd.height, (unsigned long)len, (unsigned long)(millis() - t0));
    return true;
}
