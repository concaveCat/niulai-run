/* Niulai ESP-IDF endpoint boot sequence (order is safety-critical):
 *   1. boot banner (stage + public four-leg runtime scope)
 *   2. /OE HIGH FIRST - before any other hardware init, so no failure path
 *      can leave outputs enabled
 *   3. foot_contact_init -> safety_gate_init -> ble_input_init
 *   4. watchdog task (4096 bytes, prio 10) + control task (2048 bytes, prio 6)
 *   5. UART command loop: audio play | audio stop | reset | status |
 *      transitions (the isolated I2S sine profile stops before this loop)
 *
 * The isolated voice keyword profile remains available for bring-up; the
 * normal runtime also starts its always-on telemetry listener when available.
 */
#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>

#include "ble_input.h"
#include "audio_player.h"
#include "audio_test.h"
#include "actuator_pca9685.h"
#include "control_task.h"
#include "display_test.h"
#include "display_hud.h"
#include "foot_contact.h"
#include "foot_contact_test.h"
#include "gpio39_cli.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2s_sine_test.h"
#include "i2s_loopback_test.h"
#include "i2c_bus_service.h"
#include "mpu6500_probe_test.h"
#include "runtime_imu.h"
#include "safety_gate.h"
#include "uart_log.h"
#include "voice_keyword_test.h"
#include "watchdog_task.h"
#include "qw_hal_espidf.h"

#define QW_WATCHDOG_TASK_STACK_BYTES 4096
/* The full ARD1 motion path, including actuator application, requires more
 * than the former 4096-byte control-task stack. */
#define QW_CONTROL_TASK_STACK_BYTES 8192
#define QW_WATCHDOG_TASK_PRIO 10
#define QW_CONTROL_TASK_PRIO 6

static const char *stage_name(void)
{
#if defined(CONFIG_NIULAI_MODE_BLE_ONLY)
    return "BLE_ONLY";
#elif defined(CONFIG_NIULAI_MODE_BLE_VERIFIED)
    return "BLE_VERIFIED";
#else
    return "ACTUATION_ENABLED";
#endif
}

static const char *runtime_scope_name(void)
{
    return "ALL";
}

static bool request_mama_audio_from_ble(void)
{
    /* Reuse the normal runtime's serialized voice/audio path. This only sets
     * its playback request flag; it never enters the control/FSM path. */
    return qw_voice_keyword_runtime_handle_command("NIULAI");
}

#if !defined(CONFIG_NIULAI_DISPLAY_TEST) && \
    !defined(CONFIG_NIULAI_DISPLAY_HUD_TEST) && \
    !defined(CONFIG_NIULAI_AUDIO_TEST) && \
    !defined(CONFIG_NIULAI_I2S_SINE_TEST) && \
    !defined(CONFIG_NIULAI_VOICE_KEYWORD_TEST) && \
    !defined(CONFIG_NIULAI_FOOT_CONTACT_TEST) && \
    !defined(CONFIG_NIULAI_GPIO39_CLI)
static void uart_status_command(void)
{
    qw_motion_snapshot_t motion;
    const char *state = control_fsm_snapshot(&motion)
                            ? motion.state_name
                            : "UNAVAILABLE";
    qw_uart_log_status(stage_name(), state,
                       (unsigned)safety_gate_acceptance_ticks(),
                       (unsigned)(CONFIG_NIULAI_ACTUATION_ACCEPTANCE_WINDOW_S *
                                  10),
                       safety_gate_armed() ? 1 : 0,
                       safety_gate_failsafe() ? 1 : 0);
}

static void uart_transitions_command(void)
{
    qw_motion_snapshot_t motion;
    if (!control_fsm_snapshot(&motion)) {
        return;
    }
    uint32_t n = motion.transition_count;
    qw_uart_log_transitions_header(
        (unsigned long long)motion.transition_total);
    for (uint32_t i = 0; i < n; i++) {
        const qw_motion_transition_t *t = &motion.transitions[i];
        qw_uart_log_transition(i, t->from_state, t->to_state, t->reason,
                               (unsigned)(t->elapsed_s * 1000.0f));
    }
}
#endif

void app_main(void)
{
#if !defined(CONFIG_NIULAI_GPIO39_CLI)
    qw_uart_log_boot(stage_name(), runtime_scope_name());
#endif

    /* /OE HIGH FIRST: the motion-disable invariant. Only the safety gate
     * may lower it, and only in ACTUATION_ENABLED with the runtime gate
     * open. */
    qw_hal_gpio_t boot_gpio = qw_hal_espidf_gpio();
    if (boot_gpio.configure_output == NULL ||
        boot_gpio.configure_output(boot_gpio.ctx,
                                   CONFIG_NIULAI_PCA9685_OE_PIN, true) !=
            QW_HAL_OK ||
        boot_gpio.write(boot_gpio.ctx, CONFIG_NIULAI_PCA9685_OE_PIN, true) !=
            QW_HAL_OK) {
        /* No hardware path is allowed to continue if the output-disable
         * invariant could not be established. */
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

#if defined(CONFIG_NIULAI_DISPLAY_HUD_TEST)
    /* HUD preview is isolated from BLE, FSM, audio and actuators. */
    if (qw_display_hud_demo() != ESP_OK) {
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
#elif defined(CONFIG_NIULAI_DISPLAY_TEST)
    /* Display bring-up is isolated from BLE, FSM and actuators. */
    if (qw_display_color_bar_test() != ESP_OK) {
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
#elif defined(CONFIG_NIULAI_AUDIO_TEST)
    /* Audio bring-up is isolated from BLE, FSM and actuators. */
    if (qw_audio_tone_test() != ESP_OK) {
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
#elif defined(CONFIG_NIULAI_I2S_SINE_TEST)
    /* I2S sine bring-up is isolated from the microphone, MP3, BLE, FSM and
     * actuators. */
    if (qw_i2s_sine_test() != ESP_OK) {
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
#elif defined(CONFIG_NIULAI_I2S_LOOPBACK_TEST)
    /* GPIO4 -> GPIO14 I2S loopback is isolated from the microphone keyword,
     * MP3, BLE, FSM and actuators. */
    if (qw_i2s_loopback_test() != ESP_OK) {
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#elif defined(CONFIG_NIULAI_MPU6500_PROBE_TEST)
    /* MPU6500 I2C reachability is isolated from the microphone, I2S, BLE,
     * FSM, PCA9685 and actuators. */
    if (qw_mpu6500_probe_test() != ESP_OK) {
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#elif defined(CONFIG_NIULAI_VOICE_KEYWORD_TEST)
    /* Voice bring-up is isolated from BLE, FSM, PCA9685 and actuators. */
    if (qw_voice_keyword_test_start() != ESP_OK) {
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    char voice_line[64];
    size_t voice_line_length = 0U;
    printf("UART commands: voice status | voice enroll [1|2] | voice play | voice capture | voice raw | voice clear\n");
    for (;;) {
        const int character = fgetc(stdin);
        if (character == EOF) {
            /* The default UART VFS is non-blocking.  Clear its EOF flag and
             * keep polling so a later command is not lost. */
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(10U));
            continue;
        }
        if (character == '\r' || character == '\n') {
            voice_line[voice_line_length] = '\0';
            if (strcmp(voice_line, "voice status") == 0) {
                qw_voice_keyword_test_status();
            } else if (strcmp(voice_line, "voice enroll") == 0 ||
                       strcmp(voice_line, "voice enroll 1") == 0) {
                qw_voice_keyword_test_request_enroll(1U);
            } else if (strcmp(voice_line, "voice enroll 2") == 0) {
                qw_voice_keyword_test_request_enroll(2U);
            } else if (strcmp(voice_line, "voice play") == 0) {
                qw_voice_keyword_test_request_play();
            } else if (strcmp(voice_line, "voice capture") == 0) {
                qw_voice_keyword_test_request_capture();
            } else if (strcmp(voice_line, "voice raw") == 0) {
                qw_voice_keyword_test_request_raw();
            } else if (strcmp(voice_line, "voice clear") == 0) {
                qw_voice_keyword_test_clear();
            } else if (voice_line_length != 0U) {
                qw_uart_log_unknown_command(voice_line);
            }
            voice_line_length = 0U;
            continue;
        }
        if (voice_line_length < sizeof(voice_line) - 1U) {
            voice_line[voice_line_length++] = (char)character;
        }
    }
#elif defined(CONFIG_NIULAI_FOOT_CONTACT_TEST)
    /* KW10 bring-up is isolated from BLE, FSM, PCA9685 and actuators. */
    qw_foot_contact_test_run();
#elif defined(CONFIG_NIULAI_GPIO39_CLI)
    /* GPIO39 button CLI is isolated from BLE, FSM, PCA9685 and actuators. */
    qw_gpio39_cli_run();
#else
    const esp_err_t i2c_err = qw_i2c_bus_service_init();
    if (i2c_err == ESP_OK) {
        qw_actuator_pca9685_bind_bus(qw_i2c_bus_service_bus());
    } else {
        printf("I2C_SERVICE unavailable err=%s\n", esp_err_to_name(i2c_err));
    }
    const bool contact_ready = foot_contact_init() && foot_contact_all_ready();
    safety_gate_init();
    safety_gate_notify_contact_ready(contact_ready);
    control_task_sync_init();
    ble_input_init();
    /* Accepted control packets are the heartbeat stream. Logging each one
     * synchronously on the console competes with BLE writes and notifications
     * for the same UART/GATT runtime, so keep only invalid-packet diagnostics.
     */
    ble_input_set_log_hooks(NULL, qw_uart_log_ble_ctrl_rx_invalid);
    ble_input_set_disconnect_hook(safety_gate_on_input_disconnect);

    if (xTaskCreate(watchdog_task, "watchdog", QW_WATCHDOG_TASK_STACK_BYTES,
                    NULL, QW_WATCHDOG_TASK_PRIO, NULL) != pdPASS) {
        qw_uart_log_task_create_failed("watchdog");
    }
    if (xTaskCreate(control_task, "control", QW_CONTROL_TASK_STACK_BYTES, NULL,
                    QW_CONTROL_TASK_PRIO, NULL) != pdPASS) {
        qw_uart_log_task_create_failed("control");
    }
    if (i2c_err == ESP_OK &&
        qw_runtime_imu_start(qw_i2c_bus_service_bus()) != ESP_OK) {
        printf("IMU_SERVICE unavailable telemetry_only=1\n");
    }
    if (qw_display_hud_runtime_start() != ESP_OK) {
        printf("HUD_SERVICE unavailable landscape=320x170\n");
    }
    if (qw_voice_keyword_runtime_start() != ESP_OK) {
        printf("VOICE_SERVICE unavailable always_on=0\n");
    }
    ble_input_set_audio_play_hook(request_mama_audio_from_ble);

    /* Simple UART command loop (runs at the idle-priority app_main task).
     * The default UART VFS is non-blocking: fgets() returns EOF when no
     * character is available, which would otherwise terminate this loop
     * before a later command arrives. */
    char line[64];
    size_t line_length = 0U;
    qw_uart_log_uart_prompt();
    for (;;) {
        const int character = fgetc(stdin);
        if (character == EOF) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(10U));
            continue;
        }
        if (character != '\r' && character != '\n') {
            if (line_length < sizeof(line) - 1U) {
                line[line_length++] = (char)character;
            }
            continue;
        }

        line[line_length] = '\0';
        if (qw_voice_keyword_runtime_handle_command(line) ||
            qw_audio_player_handle_command(line)) {
            line_length = 0U;
            continue;
        }
        if (strncmp(line, "reset", 5) == 0) {
            control_fsm_reset();
        } else if (strncmp(line, "status", 6) == 0) {
            uart_status_command();
        } else if (strncmp(line, "transitions", 11) == 0) {
            uart_transitions_command();
        } else if (line_length != 0U) {
            qw_uart_log_unknown_command(line);
        }
        line_length = 0U;
    }
#endif
}
