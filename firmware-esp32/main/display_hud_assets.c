#include "display_hud_assets.h"

#include <string.h>

#define QW_ASSET_BYTES (QW_HUD_LANDSCAPE_WIDTH * QW_HUD_LANDSCAPE_HEIGHT * 2U)

extern const uint8_t _binary_qw_hud_waiting_rgb565_start[];
extern const uint8_t _binary_qw_hud_waiting_rgb565_end[];
extern const uint8_t _binary_qw_hud_walking_solo_rgb565_start[];
extern const uint8_t _binary_qw_hud_walking_solo_rgb565_end[];
extern const uint8_t _binary_qw_hud_walking_multi_rgb565_start[];
extern const uint8_t _binary_qw_hud_walking_multi_rgb565_end[];

static void circle(uint16_t *fb, int cx, int cy, int radius, uint16_t color)
{
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius && cx + x >= 0 &&
                cy + y >= 0 && cx + x < (int)QW_HUD_LANDSCAPE_WIDTH &&
                cy + y < (int)QW_HUD_LANDSCAPE_HEIGHT)
                fb[(cy + y) * QW_HUD_LANDSCAPE_WIDTH + cx + x] = color;
        }
    }
}

static void draw_paw(uint16_t *fb, qw_hud_rect_t r)
{
    const uint16_t black = 0x0000;
    circle(fb, r.x + 20, r.y + 28, 12, black);
    circle(fb, r.x + 7, r.y + 10, 5, black);
    circle(fb, r.x + 16, r.y + 5, 5, black);
    circle(fb, r.x + 25, r.y + 5, 5, black);
    circle(fb, r.x + 34, r.y + 10, 5, black);
}

bool qw_hud_render_gameplay(uint16_t *framebuffer, size_t pixel_count,
                            const qw_hud_state_t *state)
{
    if (framebuffer == NULL || state == NULL || pixel_count <
        (size_t)QW_HUD_LANDSCAPE_WIDTH * QW_HUD_LANDSCAPE_HEIGHT)
        return false;
    const uint8_t *start;
    const uint8_t *end;
    if (qw_hud_is_waiting_for_link(state)) {
        start = _binary_qw_hud_waiting_rgb565_start;
        end = _binary_qw_hud_waiting_rgb565_end;
    } else if (state->game_mode == QW_HUD_GAME_MODE_MULTI) {
        start = _binary_qw_hud_walking_multi_rgb565_start;
        end = _binary_qw_hud_walking_multi_rgb565_end;
    } else {
        start = _binary_qw_hud_walking_solo_rgb565_start;
        end = _binary_qw_hud_walking_solo_rgb565_end;
    }
    if ((size_t)(end - start) < QW_ASSET_BYTES) return false;
    memcpy(framebuffer, start, QW_ASSET_BYTES);
    for (unsigned leg = 0; leg < QW_HUD_LEG_COUNT; leg++) {
        if (state->legs[leg].stepping) draw_paw(framebuffer, qw_hud_paw_rect(leg));
    }
    return true;
}
