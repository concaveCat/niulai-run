#ifndef QW_FALL_DETECTOR_H
#define QW_FALL_DETECTOR_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float trigger_angle_deg;
    float clear_angle_deg;
    uint8_t trigger_samples;
    uint8_t clear_samples;
    uint8_t over_limit_samples;
    uint8_t below_clear_samples;
    bool detected;
} qw_fall_detector_t;

void qw_fall_detector_init(qw_fall_detector_t *detector);

/* Returns the current latched-over-threshold state. A short consecutive
 * sample requirement rejects a single noisy angle without lowering the 60°
 * trigger threshold. Recovery uses hysteresis so the state cannot chatter
 * around the trigger boundary. */
bool qw_fall_detector_update(qw_fall_detector_t *detector, float roll_deg,
                             float pitch_deg, bool valid);

#endif /* QW_FALL_DETECTOR_H */
