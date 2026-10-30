#ifndef QW_DISPLAY_HUD_H
#define QW_DISPLAY_HUD_H

#include "esp_err.h"

/* Render a safe animated HUD preview; never starts BLE, FSM or actuators. */
esp_err_t qw_display_hud_demo(void);

/* Start the normal landscape HUD service. It observes runtime state only and
 * never controls actuation. */
esp_err_t qw_display_hud_runtime_start(void);

#endif /* QW_DISPLAY_HUD_H */
