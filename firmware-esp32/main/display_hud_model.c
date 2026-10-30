#include "display_hud_model.h"

#include <math.h>
#include <stdio.h>

bool qw_hud_state_valid(const qw_hud_state_t *state)
{
    return state != NULL && state->ble <= QW_HUD_BLE_WEAK &&
           state->safety <= QW_HUD_SAFETY_FAULT &&
           state->accel_state <= QW_HUD_ACCEL_HIGH &&
           state->game_mode <= QW_HUD_GAME_MODE_MULTI;
}

const char *qw_hud_ble_text(qw_hud_ble_state_t state)
{
    static const char *const labels[] = {"OFF", "LINK", "OK", "WEAK"};
    return state <= QW_HUD_BLE_WEAK ? labels[state] : "?";
}

const char *qw_hud_safety_text(qw_hud_safety_state_t state)
{
    static const char *const labels[] = {"SAFE", "WARN", "FAILSAFE", "FAULT"};
    return state <= QW_HUD_SAFETY_FAULT ? labels[state] : "?";
}

qw_hud_accel_state_t qw_hud_accel_classify(float magnitude_g,
                                           bool valid, bool clipped,
                                           uint32_t age_ms)
{
    if (!valid || clipped || age_ms > 500U || !isfinite(magnitude_g) ||
        magnitude_g < 0.0f)
        return QW_HUD_ACCEL_INVALID;
    if (magnitude_g >= 0.60f) return QW_HUD_ACCEL_HIGH;
    if (magnitude_g >= 0.25f) return QW_HUD_ACCEL_WARNING;
    return QW_HUD_ACCEL_NORMAL;
}

void qw_hud_format_accel(qw_hud_accel_state_t state, float magnitude_g,
                         char *out, size_t out_size)
{
    if (out == NULL || out_size == 0U) return;
    if (state == QW_HUD_ACCEL_INVALID) {
        (void)snprintf(out, out_size, "ACC --");
        return;
    }
    (void)snprintf(out, out_size, "ACC %.2fG", (double)magnitude_g);
}

void qw_hud_format_uptime(uint32_t uptime_seconds, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0U) {
        return;
    }
    uint32_t minutes = uptime_seconds / 60U;
    uint32_t hours = minutes / 60U;
    minutes %= 60U;
    if (hours > 99U) {
        hours = 99U;
    }
    (void)snprintf(out, out_size, "%02lu:%02lu", (unsigned long)hours,
                   (unsigned long)minutes);
}

qw_hud_rect_t qw_hud_leg_rect(unsigned leg_index)
{
    if (leg_index >= QW_HUD_LEG_COUNT) {
        return (qw_hud_rect_t){0};
    }
    return (qw_hud_rect_t){
        .x = (uint16_t)((leg_index & 1U) != 0U ? 92U : 10U),
        .y = (uint16_t)(70U + ((leg_index >= 2U) ? 88U : 0U)),
        .width = 68U,
        .height = 64U,
    };
}

qw_hud_rect_t qw_hud_landscape_leg_rect(unsigned leg_index)
{
    if (leg_index >= QW_HUD_LEG_COUNT) {
        return (qw_hud_rect_t){0};
    }
    return (qw_hud_rect_t){
        .x = (uint16_t)(8U + (leg_index * 80U)),
        .y = 56U,
        .width = 68U,
        .height = 64U,
    };
}

qw_hud_rect_t qw_hud_paw_rect(unsigned leg_index)
{
    if (leg_index >= QW_HUD_LEG_COUNT) return (qw_hud_rect_t){0};
    const bool right = (leg_index & 1U) != 0U;
    const bool bottom = leg_index >= 2U;
    return (qw_hud_rect_t){
        .x = (uint16_t)(right ? 272U : 8U),
        .y = (uint16_t)(bottom ? 120U : 8U),
        .width = 40U,
        .height = 42U,
    };
}

bool qw_hud_is_waiting_for_link(const qw_hud_state_t *state)
{
    return state != NULL && state->ble != QW_HUD_BLE_CONNECTED;
}
