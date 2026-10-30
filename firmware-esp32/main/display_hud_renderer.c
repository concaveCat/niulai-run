#include "display_hud_renderer.h"

#include <stdio.h>

#define RGB565(r, g, b)                                                       \
    ((uint16_t)((((uint16_t)(r) & 0xf8U) << 8) |                             \
                (((uint16_t)(g) & 0xfcU) << 3) | ((uint16_t)(b) >> 3)))

static const uint16_t C_BG = RGB565(10, 14, 22);
static const uint16_t C_PANEL = RGB565(24, 31, 44);
static const uint16_t C_WHITE = RGB565(245, 248, 255);
static const uint16_t C_GRAY = RGB565(100, 112, 128);
static const uint16_t C_BLUE = RGB565(55, 150, 255);
static const uint16_t C_GREEN = RGB565(44, 210, 115);
static const uint16_t C_YELLOW = RGB565(255, 190, 45);
static const uint16_t C_RED = RGB565(245, 70, 75);
static const uint8_t GLYPH_DOT[5] = {0x00, 0x00, 0x60, 0x60, 0x00};
static const uint8_t GLYPH_DASH[5] = {0x08, 0x08, 0x08, 0x08, 0x08};

static const uint8_t *glyph(char c)
{
    static const uint8_t blank[5] = {0};
    static const uint8_t table[43][5] = {
        ['0' - '0'] = {0x3e, 0x51, 0x49, 0x45, 0x3e},
        ['1' - '0'] = {0x00, 0x42, 0x7f, 0x40, 0x00},
        ['2' - '0'] = {0x42, 0x61, 0x51, 0x49, 0x46},
        ['3' - '0'] = {0x21, 0x41, 0x45, 0x4b, 0x31},
        ['4' - '0'] = {0x18, 0x14, 0x12, 0x7f, 0x10},
        ['5' - '0'] = {0x27, 0x45, 0x45, 0x45, 0x39},
        ['6' - '0'] = {0x3c, 0x4a, 0x49, 0x49, 0x30},
        ['7' - '0'] = {0x01, 0x71, 0x09, 0x05, 0x03},
        ['8' - '0'] = {0x36, 0x49, 0x49, 0x49, 0x36},
        ['9' - '0'] = {0x06, 0x49, 0x49, 0x29, 0x1e},
        ['A' - '0'] = {0x7e, 0x11, 0x11, 0x11, 0x7e},
        ['B' - '0'] = {0x7f, 0x49, 0x49, 0x49, 0x36},
        ['C' - '0'] = {0x3e, 0x41, 0x41, 0x41, 0x22},
        ['D' - '0'] = {0x7f, 0x41, 0x41, 0x22, 0x1c},
        ['E' - '0'] = {0x7f, 0x49, 0x49, 0x49, 0x41},
        ['F' - '0'] = {0x7f, 0x09, 0x09, 0x09, 0x01},
        ['G' - '0'] = {0x3e, 0x41, 0x49, 0x49, 0x7a},
        ['H' - '0'] = {0x7f, 0x08, 0x08, 0x08, 0x7f},
        ['I' - '0'] = {0x00, 0x41, 0x7f, 0x41, 0x00},
        ['J' - '0'] = {0x20, 0x40, 0x41, 0x3f, 0x01},
        ['K' - '0'] = {0x7f, 0x08, 0x14, 0x22, 0x41},
        ['L' - '0'] = {0x7f, 0x40, 0x40, 0x40, 0x40},
        ['M' - '0'] = {0x7f, 0x02, 0x0c, 0x02, 0x7f},
        ['N' - '0'] = {0x7f, 0x04, 0x08, 0x10, 0x7f},
        ['O' - '0'] = {0x3e, 0x41, 0x41, 0x41, 0x3e},
        ['P' - '0'] = {0x7f, 0x09, 0x09, 0x09, 0x06},
        ['Q' - '0'] = {0x3e, 0x41, 0x51, 0x21, 0x5e},
        ['R' - '0'] = {0x7f, 0x09, 0x19, 0x29, 0x46},
        ['S' - '0'] = {0x46, 0x49, 0x49, 0x49, 0x31},
        ['T' - '0'] = {0x01, 0x01, 0x7f, 0x01, 0x01},
        ['U' - '0'] = {0x3f, 0x40, 0x40, 0x40, 0x3f},
        ['V' - '0'] = {0x1f, 0x20, 0x40, 0x20, 0x1f},
        ['W' - '0'] = {0x7f, 0x20, 0x18, 0x20, 0x7f},
        ['X' - '0'] = {0x63, 0x14, 0x08, 0x14, 0x63},
        ['Y' - '0'] = {0x03, 0x04, 0x78, 0x04, 0x03},
        ['Z' - '0'] = {0x61, 0x51, 0x49, 0x45, 0x43},
    };
    if (c == '.') return GLYPH_DOT;
    if (c == '-') return GLYPH_DASH;
    if (c >= '0' && c <= '9') return table[c - '0'];
    if (c >= 'A' && c <= 'Z') return table[c - '0'];
    return blank;
}

uint16_t qw_hud_render_width(qw_hud_orientation_t orientation)
{
    return orientation == QW_HUD_ORIENTATION_LANDSCAPE
               ? QW_HUD_LANDSCAPE_WIDTH : QW_HUD_PORTRAIT_WIDTH;
}

uint16_t qw_hud_render_height(qw_hud_orientation_t orientation)
{
    return orientation == QW_HUD_ORIENTATION_LANDSCAPE
               ? QW_HUD_LANDSCAPE_HEIGHT : QW_HUD_PORTRAIT_HEIGHT;
}

static void pixel(uint16_t *fb, uint16_t width, uint16_t height,
                  int x, int y, uint16_t color)
{
    if (x >= 0 && y >= 0 && x < (int)width && y < (int)height)
        fb[(size_t)y * width + (size_t)x] = color;
}

static void rect(uint16_t *fb, uint16_t width, uint16_t height,
                 int x, int y, int w, int h, uint16_t color)
{
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            pixel(fb, width, height, xx, yy, color);
}

static void border(uint16_t *fb, uint16_t width, uint16_t height,
                   int x, int y, int w, int h, uint16_t color)
{
    rect(fb, width, height, x, y, w, 2, color);
    rect(fb, width, height, x, y + h - 2, w, 2, color);
    rect(fb, width, height, x, y, 2, h, color);
    rect(fb, width, height, x + w - 2, y, 2, h, color);
}

static void text(uint16_t *fb, uint16_t width, uint16_t height,
                 int x, int y, const char *s, int scale, uint16_t color)
{
    for (; *s != '\0'; s++, x += 6 * scale) {
        const uint8_t *g = glyph(*s);
        for (int col = 0; col < 5; col++)
            for (int row = 0; row < 7; row++)
                if ((g[col] & (1U << row)) != 0U)
                    rect(fb, width, height, x + col * scale,
                         y + row * scale, scale, scale, color);
    }
}

static uint16_t safety_color(qw_hud_safety_state_t safety)
{
    if (safety == QW_HUD_SAFETY_SAFE) return C_GREEN;
    if (safety == QW_HUD_SAFETY_WARNING) return C_YELLOW;
    return C_RED;
}

static uint16_t ble_color(qw_hud_ble_state_t ble)
{
    if (ble == QW_HUD_BLE_CONNECTED) return C_BLUE;
    if (ble == QW_HUD_BLE_CONNECTING || ble == QW_HUD_BLE_WEAK)
        return C_YELLOW;
    return C_GRAY;
}

static uint16_t accel_color(qw_hud_accel_state_t accel)
{
    if (accel == QW_HUD_ACCEL_NORMAL) return C_GREEN;
    if (accel == QW_HUD_ACCEL_WARNING) return C_YELLOW;
    if (accel == QW_HUD_ACCEL_HIGH) return C_RED;
    return C_GRAY;
}

static qw_hud_rect_t hud_leg_rect(unsigned leg_index,
                                  qw_hud_orientation_t orientation)
{
    return orientation == QW_HUD_ORIENTATION_LANDSCAPE
               ? qw_hud_landscape_leg_rect(leg_index)
               : qw_hud_leg_rect(leg_index);
}

bool qw_hud_render(uint16_t *fb, size_t pixel_count,
                   const qw_hud_state_t *state,
                   qw_hud_orientation_t orientation)
{
    const uint16_t width = qw_hud_render_width(orientation);
    const uint16_t height = qw_hud_render_height(orientation);
    const size_t required_pixels = (size_t)width * height;
    static const char *const names[] = {"FL", "FR", "RL", "RR"};
    char uptime[8];
    char accel_text[16];

    if (fb == NULL || pixel_count < required_pixels ||
        !qw_hud_state_valid(state))
        return false;

    rect(fb, width, height, 0, 0, width, height, C_BG);
    rect(fb, width, height, 0, 0, width, QW_HUD_STATUS_BAR_HEIGHT, C_PANEL);
    rect(fb, width, height, 7, 9, 10, 10, ble_color(state->ble));
    text(fb, width, height, 22, 10, "BLE", 1, C_WHITE);
    text(fb, width, height, 43, 10, qw_hud_ble_text(state->ble), 1,
         ble_color(state->ble));
    text(fb, width, height, 91, 10, "UP", 1, C_WHITE);
    qw_hud_format_uptime(state->uptime_seconds, uptime, sizeof(uptime));
    text(fb, width, height, 107, 10, uptime, 1, C_GREEN);

    qw_hud_format_accel(state->accel_state, state->accel_magnitude_g,
                        accel_text, sizeof(accel_text));
    if (orientation == QW_HUD_ORIENTATION_LANDSCAPE)
        text(fb, width, height, 130, 36, qw_hud_safety_text(state->safety), 2,
             safety_color(state->safety));
    else
        text(fb, width, height, 49, 40, qw_hud_safety_text(state->safety), 2,
             safety_color(state->safety));

    for (unsigned i = 0; i < QW_HUD_LEG_COUNT; i++) {
        qw_hud_rect_t r = hud_leg_rect(i, orientation);
        uint16_t edge = state->legs[i].pressed ? C_BLUE : C_GRAY;
        rect(fb, width, height, r.x, r.y, r.width, r.height, C_PANEL);
        border(fb, width, height, r.x, r.y, r.width, r.height, edge);
        text(fb, width, height, r.x + 23, r.y + 9, names[i], 2, C_WHITE);
        rect(fb, width, height, r.x + 10, r.y + 39, 10, 10,
             state->legs[i].pressed ? C_BLUE : C_GRAY);
        text(fb, width, height, r.x + 25, r.y + 41, "P", 1,
             state->legs[i].pressed ? C_BLUE : C_GRAY);
        rect(fb, width, height, r.x + 42, r.y + 39, 10, 10,
             state->legs[i].contact ? C_GREEN : C_GRAY);
        text(fb, width, height, r.x + 56, r.y + 41, "C", 1,
             state->legs[i].contact ? C_GREEN : C_GRAY);
    }

    uint16_t accel_bg = accel_color(state->accel_state);
    rect(fb, width, height, 0, height - QW_HUD_SAFETY_BAR_HEIGHT,
         width, QW_HUD_SAFETY_BAR_HEIGHT, accel_bg);
    if (orientation == QW_HUD_ORIENTATION_LANDSCAPE) {
        text(fb, width, height, 8, 144, accel_text, 1, C_BG);
        text(fb, width, height, 252, 147,
             state->oe_disabled ? "OE HIGH" : "OE LOW", 1, C_BG);
    } else {
        text(fb, width, height, 8, 296, accel_text, 1, C_BG);
        text(fb, width, height, 110, 299,
             state->oe_disabled ? "OE HIGH" : "OE LOW", 1, C_BG);
    }
    return true;
}

bool qw_hud_render_failure(uint16_t *fb, size_t pixel_count,
                           qw_hud_orientation_t orientation,
                           uint8_t blackness)
{
    const uint16_t width = qw_hud_render_width(orientation);
    const uint16_t height = qw_hud_render_height(orientation);
    const size_t required_pixels = (size_t)width * height;
    if (fb == NULL || pixel_count < required_pixels) return false;
    const uint16_t red = (uint16_t)((31U * (255U - blackness) + 127U) /
                                    255U);
    const uint16_t color = (uint16_t)(red << 11);
    for (size_t i = 0; i < required_pixels; i++) fb[i] = color;
    return true;
}
