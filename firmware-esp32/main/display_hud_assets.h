#ifndef QW_DISPLAY_HUD_ASSETS_H
#define QW_DISPLAY_HUD_ASSETS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "display_hud_model.h"

/* Render the reference 320x170 gameplay scenes and active-step paw overlays. */
bool qw_hud_render_gameplay(uint16_t *framebuffer, size_t pixel_count,
                            const qw_hud_state_t *state);

#endif /* QW_DISPLAY_HUD_ASSETS_H */
