#include "../../hal/touch_hal.h"
#include "../../hal/imu_hal.h"
#include "board.h"
#include <Arduino.h>
#include <Wire.h>
#include <TouchDrvCSTXXX.hpp>

static TouchDrvCST92xx touch;

static volatile bool     touch_data_ready = false;
static volatile bool     touch_pressed = false;
static volatile uint16_t touch_x = 0;
static volatile uint16_t touch_y = 0;

static void IRAM_ATTR touch_isr(void) {
    touch_data_ready = true;
}

void touch_hal_init(void) {
    touch.setPins(TP_RST, TP_INT);
    if (!touch.begin(Wire, CST9220_ADDR, IIC_SDA, IIC_SCL)) {
        Serial.println("Touch init failed");
        return;
    }
    touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
    touch.setSwapXY(true);
    touch.setMirrorXY(true, false);
    pinMode(TP_INT, INPUT_PULLUP);
    attachInterrupt(TP_INT, touch_isr, FALLING);
    Serial.println("Touch init OK");
}

void touch_hal_read(uint16_t* x, uint16_t* y, bool* pressed) {
    if (touch_data_ready) {
        touch_data_ready = false;
        int16_t tx[5], ty[5];
        uint8_t n = touch.getPoint(tx, ty, touch.getSupportTouchPoint());
        if (n > 0) {
            touch_pressed = true;
            touch_x = (uint16_t)tx[0];
            touch_y = (uint16_t)ty[0];
        } else {
            touch_pressed = false;
        }
    }
    // The panel image is rotated in software by IMU quadrant (display.cpp
    // rotate_strip maps logical -> physical); map the physical touch point back
    // to logical coordinates with the inverse, or taps land on the wrong widget.
    // The CST9220 frame (after setSwapXY/setMirrorXY) is itself 90° off the
    // panel's: physical = (ty, S-1-tx). Measured on hardware by tapping the
    // Música transport buttons in all four orientations.
    const uint16_t S1 = LCD_WIDTH - 1;
    const uint16_t px = touch_y, py = S1 - touch_x;
    switch (imu_hal_rotation_quadrant()) {
    case 1:  *x = py;      *y = S1 - px; break;   // inverse of (x,y) -> (S-1-y, x)
    case 2:  *x = S1 - px; *y = S1 - py; break;   // inverse of (x,y) -> (S-1-x, S-1-y)
    case 3:  *x = S1 - py; *y = px;      break;   // inverse of (x,y) -> (y, S-1-x)
    default: *x = px;      *y = py;      break;
    }
    *pressed = touch_pressed;
}
