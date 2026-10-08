#pragma once
#include <stdint.h>

enum ble_state_t {
    BLE_STATE_INIT,
    BLE_STATE_ADVERTISING,
    BLE_STATE_CONNECTED,
    BLE_STATE_DISCONNECTED,
};

void ble_init(void);
void ble_tick(void);
ble_state_t ble_get_state(void);
const char* ble_get_device_name(void);
const char* ble_get_mac_address(void);
void ble_clear_bonds(void);
bool ble_has_bonds(void);
bool ble_has_data(void);
const char* ble_get_data(void);
void ble_send_ack(void);
void ble_send_nack(void);
void ble_request_refresh(void);

// Música screen: now-playing JSON from the daemon, and transport commands back.
#define MEDIA_CMD_PREV      0x01
#define MEDIA_CMD_PLAYPAUSE 0x02
#define MEDIA_CMD_NEXT      0x03
bool ble_has_music(void);
const char* ble_get_music(void);
void ble_send_media_cmd(uint8_t cmd);

// Cover art (JPEG) reassembled from chunked writes. When a transfer completes,
// ble_get_art() returns it until ble_art_done() releases the buffer.
#define ART_MAX_BYTES (64 * 1024)
bool ble_get_art(uint16_t* id, const uint8_t** data, uint32_t* len);
void ble_art_done(void);

// BLE HID keyboard
void ble_keyboard_press(uint8_t key, uint8_t modifier);
void ble_keyboard_release(void);
