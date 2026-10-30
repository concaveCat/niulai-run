#ifndef NIULAI_SERVO_ZERO_MATH_H
#define NIULAI_SERVO_ZERO_MATH_H

#include <stdbool.h>
#include <stdint.h>

#define SERVO_ZERO_RF_STEP_UPRIGHT_HIP_US 1746u
#define SERVO_ZERO_RF_STEP_UPRIGHT_KNEE_US 1469u
/* Empirical large RF single-leg verification targets; not final calibration. */
#define SERVO_ZERO_RF_STEP_HIP_TARGET_US 1494u
#define SERVO_ZERO_RF_STEP_KNEE_PRELIFT_US 1300u
#define SERVO_ZERO_RF_STEP_KNEE_TARGET_US 1165u
#define SERVO_ZERO_RF_STEP_TOTAL_MS 1950u
#define SERVO_ZERO_AMPLITUDE_MIN_PERCENT 0u
#define SERVO_ZERO_AMPLITUDE_MAX_PERCENT 100u
#define SERVO_ZERO_REST_CHANNELS 8u
#define SERVO_ZERO_ACTIVE_FRAME_BYTES (1u + 4u * SERVO_ZERO_REST_CHANNELS)
#define SERVO_ZERO_ACTIVE_HALF_CHANNELS 4u
#define SERVO_ZERO_ACTIVE_HALF_FRAME_BYTES \
    (1u + 4u * SERVO_ZERO_ACTIVE_HALF_CHANNELS)
#define SERVO_ZERO_ACTIVE_PAIR_FRAME_BYTES (1u + 4u * 2u)
#define SERVO_ZERO_FULL_FRAME_CHANNELS 16u
#define SERVO_ZERO_FULL_FRAME_BYTES (1u + 4u * SERVO_ZERO_FULL_FRAME_CHANNELS)
#define SERVO_ZERO_REST_UPRIGHT_SETTLE_MS 400u
#define SERVO_ZERO_REST_GROUP_COUNT 4u
#define SERVO_ZERO_REST_GROUP_MOVE_MS 500u
#define SERVO_ZERO_REST_ENTER_MS \
    (SERVO_ZERO_REST_GROUP_COUNT * SERVO_ZERO_REST_GROUP_MOVE_MS)
#define SERVO_ZERO_REST_GROUP_A_MASK 0x03u
#define SERVO_ZERO_REST_GROUP_B_MASK 0x0cu
#define SERVO_ZERO_REST_GROUP_C_MASK 0x30u
#define SERVO_ZERO_REST_GROUP_D_MASK 0xc0u
#define SERVO_ZERO_REST_HOLD_MS 0u
#define SERVO_ZERO_REST_EXIT_MS 0u
#define SERVO_ZERO_REST_FINAL_SETTLE_MS 0u
#define SERVO_ZERO_REST_TOTAL_MS \
    (SERVO_ZERO_REST_UPRIGHT_SETTLE_MS + SERVO_ZERO_REST_ENTER_MS)
#define SERVO_ZERO_REST_PSEUDO_GROUP_MOVE_MS 2000u
#define SERVO_ZERO_REST_PSEUDO_PHASE_MS 120u
#define SERVO_ZERO_REST_PSEUDO_GROUP_A_MASK 0xc3u
#define SERVO_ZERO_REST_PSEUDO_GROUP_B_MASK 0x3cu
#define SERVO_ZERO_REST_PSEUDO_TOTAL_MS \
    (SERVO_ZERO_REST_UPRIGHT_SETTLE_MS + \
     SERVO_ZERO_REST_PSEUDO_PHASE_MS + \
     SERVO_ZERO_REST_PSEUDO_GROUP_MOVE_MS)
#define SERVO_ZERO_KNEE_SWING_KNEE_COUNT 4u
#define SERVO_ZERO_KNEE_SWING_NOMINAL_HALF_TRAVEL_DEG 96u
#define SERVO_ZERO_KNEE_SWING_NOMINAL_SERVO_TRAVEL_DEG 180u
#define SERVO_ZERO_KNEE_SWING_NOMINAL_SERVO_TRAVEL_US 1000u
#define SERVO_ZERO_KNEE_SWING_NOMINAL_DELTA_US \
    ((SERVO_ZERO_KNEE_SWING_NOMINAL_HALF_TRAVEL_DEG * \
      SERVO_ZERO_KNEE_SWING_NOMINAL_SERVO_TRAVEL_US + \
      SERVO_ZERO_KNEE_SWING_NOMINAL_SERVO_TRAVEL_DEG / 2u) / \
     SERVO_ZERO_KNEE_SWING_NOMINAL_SERVO_TRAVEL_DEG)
#define SERVO_ZERO_KNEE_SWING_SETTLE_MS 500u
#define SERVO_ZERO_KNEE_SWING_UPDATE_MS 20u
#define SERVO_ZERO_KNEE_SWING_MOVE_MS 760u
#define SERVO_ZERO_KNEE_SWING_MIN_US 500u
#define SERVO_ZERO_KNEE_SWING_MAX_US 2500u
#define SERVO_ZERO_CH1_COSINE_SWEEP_MIN_US 1000u
#define SERVO_ZERO_CH1_COSINE_SWEEP_MAX_US 2000u
#define SERVO_ZERO_CH1_COSINE_SWEEP_HALF_MS 2000u
#define SERVO_ZERO_CH1_COSINE_SWEEP_UPDATE_MS 20u

typedef struct {
    uint16_t hip_us;
    uint16_t knee_us;
    bool complete;
} servo_zero_rf_step_pose_t;

typedef struct {
    uint16_t pulse_us[SERVO_ZERO_REST_CHANNELS];
    bool complete;
} servo_zero_rest_pose_t;

typedef struct {
    uint16_t pulse_us[SERVO_ZERO_REST_CHANNELS];
    bool valid;
} servo_zero_knee_swing_pose_t;

typedef struct {
    uint16_t pulse_us;
    bool valid;
} servo_zero_ch1_cosine_sweep_pose_t;

uint16_t servo_zero_pulse_to_counts(uint16_t pulse_us, uint16_t frequency_hz);
uint8_t servo_zero_prescale(uint32_t oscillator_hz, uint16_t frequency_hz);
void servo_zero_make_pulse_frame(uint8_t channel, uint16_t counts,
                                 uint8_t frame[5]);
void servo_zero_make_full_off_frame(uint8_t channel, uint8_t frame[5]);
void servo_zero_make_active_frame(const uint16_t pulse_us[],
                                  uint16_t frequency_hz,
                                  uint8_t frame[SERVO_ZERO_ACTIVE_FRAME_BYTES]);
void servo_zero_make_active_half_frame(
    const uint16_t pulse_us[], uint8_t first_channel, uint16_t frequency_hz,
    uint8_t frame[SERVO_ZERO_ACTIVE_HALF_FRAME_BYTES]);
void servo_zero_make_active_pair_frame(uint16_t hip_us, uint16_t knee_us,
                                       uint16_t frequency_hz,
                                       uint8_t frame[SERVO_ZERO_ACTIVE_PAIR_FRAME_BYTES]);
/* Build a target-only frame for the global-OE PCA9685 bench test. CH0/CH1
 * receive PWM; every other PCA9685 channel is explicitly FULL_OFF. */
void servo_zero_make_active_pair_only_frame(
    uint16_t hip_us, uint16_t knee_us, uint16_t frequency_hz,
    uint8_t frame[SERVO_ZERO_FULL_FRAME_BYTES]);
void servo_zero_make_active_with_unused_off_frame(
    const uint16_t pulse_us[], uint16_t frequency_hz,
    uint8_t frame[SERVO_ZERO_FULL_FRAME_BYTES]);
uint16_t servo_zero_s_curve_lerp(uint16_t start_us, uint16_t end_us,
                                 uint32_t elapsed_ms, uint32_t duration_ms);
uint16_t servo_zero_minimum_jerk_lerp(uint16_t start_us, uint16_t end_us,
                                      uint32_t elapsed_ms,
                                      uint32_t duration_ms);
uint16_t servo_zero_scale_pulse(uint16_t start_us, uint16_t target_us,
                                uint8_t amplitude_percent);
servo_zero_rf_step_pose_t servo_zero_rf_step_pose_at_amplitude(
    uint32_t elapsed_ms, uint16_t upright_hip_us, uint16_t upright_knee_us,
    uint8_t amplitude_percent);
servo_zero_rf_step_pose_t servo_zero_rf_step_pose(uint32_t elapsed_ms,
                                                  uint16_t upright_hip_us,
                                                  uint16_t upright_knee_us);
uint16_t servo_zero_rest_upright_us(uint8_t channel);
uint16_t servo_zero_rest_target_us(uint8_t channel);
uint16_t servo_zero_rest_target_us_at_amplitude(uint8_t channel,
                                                uint8_t amplitude_percent);
servo_zero_rest_pose_t servo_zero_rest_pose_at_amplitude(
    uint32_t elapsed_ms, uint8_t amplitude_percent);
servo_zero_rest_pose_t servo_zero_rest_pose(uint32_t elapsed_ms);
servo_zero_rest_pose_t servo_zero_rest_pseudo_pose_at_amplitude(
    uint32_t elapsed_ms, uint8_t amplitude_percent);
uint16_t servo_zero_knee_swing_delta_us(uint8_t amplitude_percent);
servo_zero_knee_swing_pose_t servo_zero_knee_swing_pose_at_amplitude(
    uint32_t elapsed_ms, uint8_t amplitude_percent);
/* Start continuously from the current CH1 pulse, ease to 1000 us, then
 * repeat 1000 -> 2000 -> 1000 us.  The first leg and every half swing use a
 * cosine half-wave with zero velocity at both endpoints. */
servo_zero_ch1_cosine_sweep_pose_t servo_zero_ch1_cosine_sweep_pose(
    uint32_t elapsed_ms, uint16_t initial_us);

#endif
