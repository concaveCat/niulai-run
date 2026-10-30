#include "display_hud.h"

#include <stdlib.h>

#include "display_hud_model.h"
#include "display_hud_renderer.h"
#include "display_hud_assets.h"
#include "display_service.h"
#include "ble_input.h"
#include "control_task.h"
#include "foot_contact.h"
#include "safety_gate.h"
#include "runtime_imu.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "st7789.h"

static const char *TAG = "display_hud";

#if defined(CONFIG_NIULAI_DISPLAY_HUD_LANDSCAPE)
#define HUD_ORIENTATION QW_HUD_ORIENTATION_LANDSCAPE
#define HUD_WIDTH QW_HUD_LANDSCAPE_WIDTH
#define HUD_HEIGHT QW_HUD_LANDSCAPE_HEIGHT
#define HUD_X_OFFSET CONFIG_NIULAI_DISPLAY_HUD_LANDSCAPE_X_OFFSET
#define HUD_Y_OFFSET CONFIG_NIULAI_DISPLAY_HUD_LANDSCAPE_Y_OFFSET
#define HUD_MADCTL CONFIG_NIULAI_DISPLAY_HUD_LANDSCAPE_MADCTL
#else
#define HUD_ORIENTATION QW_HUD_ORIENTATION_PORTRAIT
#define HUD_WIDTH QW_HUD_PORTRAIT_WIDTH
#define HUD_HEIGHT QW_HUD_PORTRAIT_HEIGHT
#define HUD_X_OFFSET CONFIG_NIULAI_DISPLAY_X_OFFSET
#define HUD_Y_OFFSET CONFIG_NIULAI_DISPLAY_Y_OFFSET
#define HUD_MADCTL CONFIG_NIULAI_DISPLAY_MADCTL
#endif

static st7789_config_t display_config(void)
{
    st7789_config_t c = st7789_config_default();
    c.sclk_gpio = CONFIG_NIULAI_DISPLAY_SCLK_GPIO;
    c.mosi_gpio = CONFIG_NIULAI_DISPLAY_MOSI_GPIO;
    c.cs_gpio = CONFIG_NIULAI_DISPLAY_CS_GPIO;
    c.dc_gpio = CONFIG_NIULAI_DISPLAY_DC_GPIO;
    c.reset_gpio = CONFIG_NIULAI_DISPLAY_RESET_GPIO;
    c.backlight_gpio = CONFIG_NIULAI_DISPLAY_BACKLIGHT_GPIO;
    c.width = HUD_WIDTH;
    c.height = HUD_HEIGHT;
    c.x_offset = HUD_X_OFFSET;
    c.y_offset = HUD_Y_OFFSET;
    c.madctl = HUD_MADCTL;
#if defined(CONFIG_NIULAI_DISPLAY_BACKLIGHT_ACTIVE_HIGH)
    c.backlight_active_high = true;
#else
    c.backlight_active_high = false;
#endif
    return c;
}

static qw_display_service_t s_runtime_display;
static uint16_t *s_runtime_framebuffer;
static TaskHandle_t s_runtime_task;
static bool s_failure_animation_active;
static bool s_previous_fall_detected;
static int64_t s_failure_started_us;

static st7789_config_t runtime_display_config(void)
{
    st7789_config_t config = st7789_config_default();
    config.sclk_gpio = CONFIG_NIULAI_DISPLAY_SCLK_GPIO;
    config.mosi_gpio = CONFIG_NIULAI_DISPLAY_MOSI_GPIO;
    config.cs_gpio = CONFIG_NIULAI_DISPLAY_CS_GPIO;
    config.dc_gpio = CONFIG_NIULAI_DISPLAY_DC_GPIO;
    config.reset_gpio = CONFIG_NIULAI_DISPLAY_RESET_GPIO;
    config.backlight_gpio = CONFIG_NIULAI_DISPLAY_BACKLIGHT_GPIO;
    config.width = QW_HUD_LANDSCAPE_WIDTH;
    config.height = QW_HUD_LANDSCAPE_HEIGHT;
    config.x_offset = 0U;
    config.y_offset = 35U;
    config.madctl = 0x60U;
#if defined(CONFIG_NIULAI_DISPLAY_BACKLIGHT_ACTIVE_HIGH)
    config.backlight_active_high = true;
#else
    config.backlight_active_high = false;
#endif
    return config;
}

static qw_hud_safety_state_t runtime_safety_state(
    const qw_motion_snapshot_t *motion, const remote_leg_input_t *input)
{
    if (input != NULL && input->failsafe_active) {
        return QW_HUD_SAFETY_FAILSAFE;
    }
    if (motion != NULL && motion->fault) {
        return QW_HUD_SAFETY_FAULT;
    }
    return safety_gate_armed() ? QW_HUD_SAFETY_SAFE : QW_HUD_SAFETY_WARNING;
}

static void runtime_display_task(void *arg)
{
    (void)arg;
    for (;;) {
        const int64_t now_us = esp_timer_get_time();
        const bool fall_detected = qw_runtime_imu_fall_detected();
        if (fall_detected && !s_previous_fall_detected) {
            s_failure_animation_active = true;
            s_failure_started_us = now_us;
        }
        s_previous_fall_detected = fall_detected;

        if (s_failure_animation_active) {
            const int64_t elapsed_us = now_us - s_failure_started_us;
            if (elapsed_us < 1000000LL) {
                const uint8_t blackness = (uint8_t)((elapsed_us * 255LL) /
                                                     1000000LL);
                if (!qw_hud_render_failure(
                        s_runtime_framebuffer,
                        (size_t)QW_HUD_LANDSCAPE_WIDTH *
                            QW_HUD_LANDSCAPE_HEIGHT,
                        QW_HUD_ORIENTATION_LANDSCAPE, blackness) ||
                    qw_display_service_present(
                        &s_runtime_display, s_runtime_framebuffer,
                        (size_t)QW_HUD_LANDSCAPE_WIDTH *
                            QW_HUD_LANDSCAPE_HEIGHT) != ESP_OK) {
                    ESP_LOGW(TAG, "runtime HUD failure frame present failed");
                }
                vTaskDelay(pdMS_TO_TICKS(40U));
                continue;
            }
            s_failure_animation_active = false;
        }

        const ble_input_gate_snapshot_t gate = ble_input_gate_snapshot();
        qw_runtime_imu_snapshot_t imu = {0};
        const bool have_imu = qw_runtime_imu_snapshot(&imu);
        qw_motion_snapshot_t motion = {0};
        const bool have_motion = control_fsm_snapshot(&motion);
        qw_hud_state_t state = {
            .ble = gate.link_secure
                       ? QW_HUD_BLE_CONNECTED
                       : gate.input.has_received ? QW_HUD_BLE_WEAK
                                                  : QW_HUD_BLE_DISCONNECTED,
            .safety = runtime_safety_state(have_motion ? &motion : NULL,
                                            &gate.input),
            .uptime_seconds = (uint32_t)(esp_timer_get_time() / 1000000LL),
            .oe_disabled = !safety_gate_armed(),
            .accel_state = qw_hud_accel_classify(
                imu.accel_magnitude_g, have_imu, imu.clipped, imu.age_ms),
            .accel_magnitude_g = imu.accel_magnitude_g,
            .game_mode = ble_input_game_mode() == QW_BLE_GAME_MODE_MULTI
                             ? QW_HUD_GAME_MODE_MULTI
                             : QW_HUD_GAME_MODE_SOLO,
        };
        const uint8_t contact = foot_contact_mask();
        for (uint8_t leg = 0; leg < QW_HUD_LEG_COUNT; leg++) {
            state.legs[leg].pressed =
                (gate.input.leg_mask & (uint8_t)(1U << leg)) != 0U;
            state.legs[leg].contact =
                (contact & (uint8_t)(1U << leg)) != 0U;
            state.legs[leg].stepping =
                have_motion &&
                (motion.stepping_leg_mask & (uint8_t)(1U << leg)) != 0U;
        }
        if (qw_hud_render_gameplay(s_runtime_framebuffer,
                          (size_t)QW_HUD_LANDSCAPE_WIDTH *
                              QW_HUD_LANDSCAPE_HEIGHT,
                          &state) &&
            qw_display_service_present(
                &s_runtime_display, s_runtime_framebuffer,
                (size_t)QW_HUD_LANDSCAPE_WIDTH * QW_HUD_LANDSCAPE_HEIGHT) !=
                ESP_OK) {
            ESP_LOGW(TAG, "runtime HUD present failed");
        }
        vTaskDelay(pdMS_TO_TICKS(250U));
    }
}

esp_err_t qw_display_hud_runtime_start(void)
{
    if (s_runtime_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    const st7789_config_t config = runtime_display_config();
    esp_err_t err = qw_display_service_init(
        &config, QW_DISPLAY_LANDSCAPE, &s_runtime_display);
    if (err != ESP_OK) {
        return err;
    }
    s_runtime_framebuffer = malloc((size_t)QW_HUD_LANDSCAPE_WIDTH *
                                   QW_HUD_LANDSCAPE_HEIGHT *
                                   sizeof(*s_runtime_framebuffer));
    if (s_runtime_framebuffer == NULL) {
        qw_display_service_deinit(&s_runtime_display);
        return ESP_ERR_NO_MEM;
    }
    err = qw_display_service_set_backlight(&s_runtime_display, true);
    s_failure_animation_active = false;
    s_previous_fall_detected = false;
    s_failure_started_us = 0;
    if (err == ESP_OK &&
        xTaskCreate(runtime_display_task, "runtime_hud", 4096U, NULL, 3U,
                    &s_runtime_task) != pdPASS) {
        err = ESP_ERR_NO_MEM;
    }
    if (err != ESP_OK) {
        free(s_runtime_framebuffer);
        s_runtime_framebuffer = NULL;
        qw_display_service_deinit(&s_runtime_display);
    }
    return err;
}

esp_err_t qw_display_hud_demo(void)
{
    static const qw_hud_state_t frames[] = {
        {.ble=QW_HUD_BLE_DISCONNECTED,.safety=QW_HUD_SAFETY_SAFE,.oe_disabled=true},
        {.ble=QW_HUD_BLE_CONNECTING,.safety=QW_HUD_SAFETY_SAFE,.uptime_seconds=17,.oe_disabled=true},
        {.ble=QW_HUD_BLE_CONNECTED,.safety=QW_HUD_SAFETY_SAFE,.uptime_seconds=83,.oe_disabled=true,
         .legs={{true,false,false},{false,true,false},{true,true,true},
                {false,false,false}}},
        {.ble=QW_HUD_BLE_WEAK,.safety=QW_HUD_SAFETY_FAILSAFE,.uptime_seconds=3661,.oe_disabled=true},
    };
    st7789_config_t config = display_config();
    qw_display_service_t display = {0};
    uint16_t *fb = malloc((size_t)HUD_WIDTH * HUD_HEIGHT * sizeof(*fb));
    if (fb == NULL) return ESP_ERR_NO_MEM;
    esp_err_t err = qw_display_service_init(
        &config, HUD_ORIENTATION == QW_HUD_ORIENTATION_LANDSCAPE
                     ? QW_DISPLAY_LANDSCAPE
                     : QW_DISPLAY_PORTRAIT,
        &display);
    if (err == ESP_OK) err = qw_display_service_set_backlight(&display, true);
    for (size_t i = 0; err == ESP_OK;
         i = (i + 1U) % (sizeof(frames) / sizeof(frames[0]))) {
        if (!qw_hud_render(fb, (size_t)HUD_WIDTH * HUD_HEIGHT, &frames[i],
                           HUD_ORIENTATION)) {
            err = ESP_ERR_INVALID_ARG;
            break;
        }
        err = qw_display_service_present(&display, fb,
                                         (size_t)HUD_WIDTH * HUD_HEIGHT);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "HUD frame: BLE=%s SAFETY=%s UPTIME=%lu s OE=HIGH",
                     qw_hud_ble_text(frames[i].ble),
                     qw_hud_safety_text(frames[i].safety),
                     (unsigned long)frames[i].uptime_seconds);
            vTaskDelay(pdMS_TO_TICKS(2500));
        }
    }
    free(fb);
    qw_display_service_deinit(&display);
    return err;
}
