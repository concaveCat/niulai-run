#ifndef QW_DISPLAY_HUD_MODEL_H
#define QW_DISPLAY_HUD_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define QW_HUD_PORTRAIT_WIDTH 170U
#define QW_HUD_PORTRAIT_HEIGHT 320U
#define QW_HUD_LANDSCAPE_WIDTH 320U
#define QW_HUD_LANDSCAPE_HEIGHT 170U
#define QW_HUD_WIDTH QW_HUD_PORTRAIT_WIDTH
#define QW_HUD_HEIGHT QW_HUD_PORTRAIT_HEIGHT
#define QW_HUD_STATUS_BAR_HEIGHT 28U
#define QW_HUD_SAFETY_BAR_HEIGHT 36U
#define QW_HUD_LEG_COUNT 4U

typedef enum {
    QW_HUD_BLE_DISCONNECTED = 0,
    QW_HUD_BLE_CONNECTING,
    QW_HUD_BLE_CONNECTED,
    QW_HUD_BLE_WEAK,
} qw_hud_ble_state_t;

typedef enum {
    QW_HUD_SAFETY_SAFE = 0,
    QW_HUD_SAFETY_WARNING,
    QW_HUD_SAFETY_FAILSAFE,
    QW_HUD_SAFETY_FAULT,
} qw_hud_safety_state_t;

typedef enum {
    QW_HUD_ACCEL_INVALID = 0,
    QW_HUD_ACCEL_NORMAL,
    QW_HUD_ACCEL_WARNING,
    QW_HUD_ACCEL_HIGH,
} qw_hud_accel_state_t;

typedef enum {
    QW_HUD_GAME_MODE_SOLO = 0,
    QW_HUD_GAME_MODE_MULTI,
} qw_hud_game_mode_t;

typedef struct {
    bool pressed;
    bool contact;
    bool stepping;
} qw_hud_leg_state_t;

typedef struct {
    qw_hud_ble_state_t ble;
    qw_hud_safety_state_t safety;
    uint32_t uptime_seconds;
    bool oe_disabled;
    qw_hud_accel_state_t accel_state;
    float accel_magnitude_g;
    /* Display-only. This field is never read by control or safety code. */
    qw_hud_game_mode_t game_mode;
    qw_hud_leg_state_t legs[QW_HUD_LEG_COUNT];
} qw_hud_state_t;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} qw_hud_rect_t;

bool qw_hud_state_valid(const qw_hud_state_t *state);
const char *qw_hud_ble_text(qw_hud_ble_state_t state);
const char *qw_hud_safety_text(qw_hud_safety_state_t state);
qw_hud_accel_state_t qw_hud_accel_classify(float magnitude_g,
                                           bool valid, bool clipped,
                                           uint32_t age_ms);
void qw_hud_format_accel(qw_hud_accel_state_t state, float magnitude_g,
                         char *out, size_t out_size);
void qw_hud_format_uptime(uint32_t uptime_seconds, char *out, size_t out_size);
qw_hud_rect_t qw_hud_leg_rect(unsigned leg_index);
qw_hud_rect_t qw_hud_landscape_leg_rect(unsigned leg_index);
qw_hud_rect_t qw_hud_paw_rect(unsigned leg_index);
bool qw_hud_is_waiting_for_link(const qw_hud_state_t *state);

#endif /* QW_DISPLAY_HUD_MODEL_H */
