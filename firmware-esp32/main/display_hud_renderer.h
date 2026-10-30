#ifndef QW_DISPLAY_HUD_RENDERER_H
#define QW_DISPLAY_HUD_RENDERER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "display_hud_model.h"

typedef enum {
    QW_HUD_ORIENTATION_PORTRAIT = 0,
    QW_HUD_ORIENTATION_LANDSCAPE,
} qw_hud_orientation_t;

uint16_t qw_hud_render_width(qw_hud_orientation_t orientation);
uint16_t qw_hud_render_height(qw_hud_orientation_t orientation);

/*
 * Render one complete HUD frame into an RGB565 framebuffer. The framebuffer
 * is not byte-swapped; the ST7789 transport owns the wire-order conversion.
 */
bool qw_hud_render(uint16_t *framebuffer, size_t pixel_count,
                   const qw_hud_state_t *state,
                   qw_hud_orientation_t orientation);

/* Render the failure transition: blackness 0 is full red, 255 is black. */
bool qw_hud_render_failure(uint16_t *framebuffer, size_t pixel_count,
                           qw_hud_orientation_t orientation,
                           uint8_t blackness);

#endif /* QW_DISPLAY_HUD_RENDERER_H */
