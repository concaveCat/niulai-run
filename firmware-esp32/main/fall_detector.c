#include "fall_detector.h"

#include <math.h>
#include <stddef.h>

#define QW_FALL_TRIGGER_ANGLE_DEG 60.0f
#define QW_FALL_CLEAR_ANGLE_DEG 50.0f
#define QW_FALL_TRIGGER_SAMPLES 3U
#define QW_FALL_CLEAR_SAMPLES 25U

void qw_fall_detector_init(qw_fall_detector_t *detector)
{
    if (detector == NULL) return;
    *detector = (qw_fall_detector_t){
        .trigger_angle_deg = QW_FALL_TRIGGER_ANGLE_DEG,
        .clear_angle_deg = QW_FALL_CLEAR_ANGLE_DEG,
        .trigger_samples = QW_FALL_TRIGGER_SAMPLES,
        .clear_samples = QW_FALL_CLEAR_SAMPLES,
    };
}

bool qw_fall_detector_update(qw_fall_detector_t *detector, float roll_deg,
                             float pitch_deg, bool valid)
{
    if (detector == NULL) return false;
    if (!valid || !isfinite(roll_deg) || !isfinite(pitch_deg)) {
        detector->over_limit_samples = 0U;
        detector->below_clear_samples = 0U;
        return detector->detected;
    }

    const bool over_limit = fabsf(roll_deg) > detector->trigger_angle_deg ||
                            fabsf(pitch_deg) > detector->trigger_angle_deg;
    if (!detector->detected) {
        detector->below_clear_samples = 0U;
        if (over_limit) {
            if (detector->over_limit_samples < UINT8_MAX) {
                detector->over_limit_samples++;
            }
            if (detector->over_limit_samples >= detector->trigger_samples) {
                detector->detected = true;
                detector->over_limit_samples = 0U;
            }
        } else {
            detector->over_limit_samples = 0U;
        }
        return detector->detected;
    }

    detector->over_limit_samples = 0U;
    const bool below_clear = fabsf(roll_deg) < detector->clear_angle_deg &&
                             fabsf(pitch_deg) < detector->clear_angle_deg;
    if (below_clear) {
        if (detector->below_clear_samples < UINT8_MAX) {
            detector->below_clear_samples++;
        }
        if (detector->below_clear_samples >= detector->clear_samples) {
            detector->detected = false;
            detector->below_clear_samples = 0U;
        }
    } else {
        detector->below_clear_samples = 0U;
    }
    return detector->detected;
}
