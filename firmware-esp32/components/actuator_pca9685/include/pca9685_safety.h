#ifndef NIULAI_PCA9685_SAFETY_H
#define NIULAI_PCA9685_SAFETY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* These defaults are the narrowest software envelope supported by the
 * current four-leg bench evidence. Kconfig can tighten or recalibrate them,
 * but it cannot widen a channel past the PCA9685 transport envelope. */
#ifndef CONFIG_NIULAI_FORMAL_CH0_MIN_US
#define CONFIG_NIULAI_FORMAL_CH0_MIN_US 1400
#endif
#ifndef CONFIG_NIULAI_FORMAL_CH0_MAX_US
#define CONFIG_NIULAI_FORMAL_CH0_MAX_US 2500
#endif
#ifndef CONFIG_NIULAI_FORMAL_CH1_MIN_US
#define CONFIG_NIULAI_FORMAL_CH1_MIN_US 500
#endif
#ifndef CONFIG_NIULAI_FORMAL_CH1_MAX_US
#define CONFIG_NIULAI_FORMAL_CH1_MAX_US 2500
#endif
#ifndef CONFIG_NIULAI_FORMAL_CH2_MIN_US
#define CONFIG_NIULAI_FORMAL_CH2_MIN_US 500
#endif
#ifndef CONFIG_NIULAI_FORMAL_CH2_MAX_US
#define CONFIG_NIULAI_FORMAL_CH2_MAX_US 1750
#endif
#ifndef CONFIG_NIULAI_FORMAL_CH3_MIN_US
#define CONFIG_NIULAI_FORMAL_CH3_MIN_US 500
#endif
#ifndef CONFIG_NIULAI_FORMAL_CH3_MAX_US
#define CONFIG_NIULAI_FORMAL_CH3_MAX_US 2500
#endif
#ifndef CONFIG_NIULAI_FORMAL_CH4_MIN_US
#define CONFIG_NIULAI_FORMAL_CH4_MIN_US 500
#endif
#ifndef CONFIG_NIULAI_FORMAL_CH4_MAX_US
#define CONFIG_NIULAI_FORMAL_CH4_MAX_US 1940
#endif
#ifndef CONFIG_NIULAI_FORMAL_CH5_MIN_US
#define CONFIG_NIULAI_FORMAL_CH5_MIN_US 500
#endif
#ifndef CONFIG_NIULAI_FORMAL_CH5_MAX_US
#define CONFIG_NIULAI_FORMAL_CH5_MAX_US 2500
#endif
#ifndef CONFIG_NIULAI_FORMAL_CH6_MIN_US
#define CONFIG_NIULAI_FORMAL_CH6_MIN_US 1120
#endif
#ifndef CONFIG_NIULAI_FORMAL_CH6_MAX_US
#define CONFIG_NIULAI_FORMAL_CH6_MAX_US 2500
#endif
#ifndef CONFIG_NIULAI_FORMAL_CH7_MIN_US
#define CONFIG_NIULAI_FORMAL_CH7_MIN_US 500
#endif
#ifndef CONFIG_NIULAI_FORMAL_CH7_MAX_US
#define CONFIG_NIULAI_FORMAL_CH7_MAX_US 2500
#endif

#define QW_PCA9685_FORMAL_CHANNEL_COUNT 8U
#define QW_PCA9685_TRANSPORT_MIN_US 500
#define QW_PCA9685_TRANSPORT_MAX_US 2500

typedef bool (*qw_pca9685_safety_step_t)(void *ctx);

/* Execute all three emergency-stop steps even when an earlier step fails.
 * This small platform-independent contract is deliberately used by the
 * driver so failure-combination tests cannot accidentally regress into
 * short-circuit evaluation. A NULL step is unconfirmed and contributes false,
 * but does not suppress later steps. */
static inline bool qw_pca9685_run_safe_stop_steps(
    qw_pca9685_safety_step_t oe_step,
    qw_pca9685_safety_step_t rest_step,
    qw_pca9685_safety_step_t sleep_step,
    void *ctx)
{
    const bool oe_ok = oe_step != NULL && oe_step(ctx);
    const bool rest_ok = rest_step != NULL && rest_step(ctx);
    const bool sleep_ok = sleep_step != NULL && sleep_step(ctx);
    return oe_ok && rest_ok && sleep_ok;
}

static inline bool qw_pca9685_limits_valid(void)
{
    static const int32_t hard_minimum[QW_PCA9685_FORMAL_CHANNEL_COUNT] = {
        1400, 500, 500, 500, 500, 500, 1120, 500,
    };
    static const int32_t hard_maximum[QW_PCA9685_FORMAL_CHANNEL_COUNT] = {
        2500, 2500, 1750, 2500, 1940, 2500, 2500,
        2500,
    };
    static const int32_t minimum[QW_PCA9685_FORMAL_CHANNEL_COUNT] = {
        CONFIG_NIULAI_FORMAL_CH0_MIN_US,
        CONFIG_NIULAI_FORMAL_CH1_MIN_US,
        CONFIG_NIULAI_FORMAL_CH2_MIN_US,
        CONFIG_NIULAI_FORMAL_CH3_MIN_US,
        CONFIG_NIULAI_FORMAL_CH4_MIN_US,
        CONFIG_NIULAI_FORMAL_CH5_MIN_US,
        CONFIG_NIULAI_FORMAL_CH6_MIN_US,
        CONFIG_NIULAI_FORMAL_CH7_MIN_US,
    };
    static const int32_t maximum[QW_PCA9685_FORMAL_CHANNEL_COUNT] = {
        CONFIG_NIULAI_FORMAL_CH0_MAX_US,
        CONFIG_NIULAI_FORMAL_CH1_MAX_US,
        CONFIG_NIULAI_FORMAL_CH2_MAX_US,
        CONFIG_NIULAI_FORMAL_CH3_MAX_US,
        CONFIG_NIULAI_FORMAL_CH4_MAX_US,
        CONFIG_NIULAI_FORMAL_CH5_MAX_US,
        CONFIG_NIULAI_FORMAL_CH6_MAX_US,
        CONFIG_NIULAI_FORMAL_CH7_MAX_US,
    };
    for (uint8_t channel = 0U;
         channel < QW_PCA9685_FORMAL_CHANNEL_COUNT; channel++) {
        if (minimum[channel] < hard_minimum[channel] ||
            maximum[channel] > hard_maximum[channel] ||
            minimum[channel] > maximum[channel]) {
            return false;
        }
    }
    return true;
}

static inline bool qw_pca9685_pulse_within_limit(uint8_t channel,
                                                  int32_t pulse_us)
{
    static const int32_t hard_minimum[QW_PCA9685_FORMAL_CHANNEL_COUNT] = {
        1400, 500, 500, 500, 500, 500, 1120, 500,
    };
    static const int32_t hard_maximum[QW_PCA9685_FORMAL_CHANNEL_COUNT] = {
        2500, 2500, 1750, 2500, 1940, 2500, 2500,
        2500,
    };
    static const int32_t minimum[QW_PCA9685_FORMAL_CHANNEL_COUNT] = {
        CONFIG_NIULAI_FORMAL_CH0_MIN_US,
        CONFIG_NIULAI_FORMAL_CH1_MIN_US,
        CONFIG_NIULAI_FORMAL_CH2_MIN_US,
        CONFIG_NIULAI_FORMAL_CH3_MIN_US,
        CONFIG_NIULAI_FORMAL_CH4_MIN_US,
        CONFIG_NIULAI_FORMAL_CH5_MIN_US,
        CONFIG_NIULAI_FORMAL_CH6_MIN_US,
        CONFIG_NIULAI_FORMAL_CH7_MIN_US,
    };
    static const int32_t maximum[QW_PCA9685_FORMAL_CHANNEL_COUNT] = {
        CONFIG_NIULAI_FORMAL_CH0_MAX_US,
        CONFIG_NIULAI_FORMAL_CH1_MAX_US,
        CONFIG_NIULAI_FORMAL_CH2_MAX_US,
        CONFIG_NIULAI_FORMAL_CH3_MAX_US,
        CONFIG_NIULAI_FORMAL_CH4_MAX_US,
        CONFIG_NIULAI_FORMAL_CH5_MAX_US,
        CONFIG_NIULAI_FORMAL_CH6_MAX_US,
        CONFIG_NIULAI_FORMAL_CH7_MAX_US,
    };
    return channel < QW_PCA9685_FORMAL_CHANNEL_COUNT &&
           pulse_us >= hard_minimum[channel] &&
           pulse_us <= hard_maximum[channel] &&
           pulse_us >= minimum[channel] && pulse_us <= maximum[channel] &&
           pulse_us >= QW_PCA9685_TRANSPORT_MIN_US &&
           pulse_us <= QW_PCA9685_TRANSPORT_MAX_US;
}

#endif /* NIULAI_PCA9685_SAFETY_H */
