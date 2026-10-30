#!/bin/sh
# Niulai firmware-esp32 host test suite (M2 gate).
# Compiles the four pure-C components with plain gcc - no ESP toolchain -
# runs every test binary, then greps the pure-C sources for forbidden
# tokens (purity red line). Any failure exits non-zero.
set -eu
cd "$(dirname "$0")"
trap 'rm -f test_leg_fsm.bin test_control_glue.bin test_motion_strategy.bin test_motion_ard1.bin test_safety_policy.bin test_pca9685_safety.bin test_pca9685_safety_rejects_widening.bin test_safety_contracts.bin test_pca9685_driver_stop.bin test_ble_protocol.bin test_remote_input_state.bin test_display_hud_model.bin test_display_hud_renderer.bin test_display_service.bin test_st7789_driver.bin test_ble_endpoint_mock.bin test_mpu6500_math.bin test_fall_detector.bin test_foot_contact.bin test_foot_contact_debounce.bin test_servo_interpolator.bin test_voice_keyword.bin test_audio_command.bin test_i2s_sine_command.bin test_mp3_decoder.bin test_qw_hal.bin' EXIT

ROOT=../..
INC="-I$ROOT/components/leg_fsm/include -I$ROOT/components/control_glue/include -I$ROOT/components/ble_input/include"
MOTION_INC="-I$ROOT/components/motion_strategy/include"
ARD1_INC="-I$ROOT/components/ard1_core/include"
CONTACT_INC="-I$ROOT/components/foot_contact/include"
VOICE_INC="-I$ROOT/components/voice_keyword/include"
MP3_INC="-I$ROOT/components/mp3_decoder/include"
MP3_SRCS="$ROOT/components/mp3_decoder/src/bitstream.c $ROOT/components/mp3_decoder/src/buffers.c $ROOT/components/mp3_decoder/src/dct32.c $ROOT/components/mp3_decoder/src/dequant.c $ROOT/components/mp3_decoder/src/dqchan.c $ROOT/components/mp3_decoder/src/huffman.c $ROOT/components/mp3_decoder/src/hufftabs.c $ROOT/components/mp3_decoder/src/imdct.c $ROOT/components/mp3_decoder/src/mp3dec.c $ROOT/components/mp3_decoder/src/mp3tabs.c $ROOT/components/mp3_decoder/src/polyphase.c $ROOT/components/mp3_decoder/src/scalfact.c $ROOT/components/mp3_decoder/src/stproc.c $ROOT/components/mp3_decoder/src/subband.c $ROOT/components/mp3_decoder/src/trigtabs.c"
HAL_INC="-I$ROOT/components/qw_hal/include"
PURE_SRCS="$ROOT/components/leg_fsm/leg_fsm.c $ROOT/components/control_glue/control_glue.c $ROOT/components/ble_input/ble_protocol.c $ROOT/components/ble_input/remote_input_state.c"
WARN="-std=c11 -Wall -Wextra -Werror"

echo "== compiling =="
gcc $WARN $INC -o test_leg_fsm.bin test_leg_fsm.c $PURE_SRCS -lm
gcc $WARN $INC -o test_control_glue.bin test_control_glue.c $PURE_SRCS -lm
gcc $WARN $INC $MOTION_INC -o test_motion_strategy.bin test_motion_strategy.c \
    $ROOT/components/motion_strategy/motion_strategy.c \
    $ROOT/components/motion_strategy/motion_strategy_leg_fsm.c \
    $ROOT/components/leg_fsm/leg_fsm.c $ROOT/components/control_glue/control_glue.c -lm
gcc $WARN $INC $MOTION_INC $ARD1_INC -o test_motion_ard1.bin test_motion_ard1.c \
    $ROOT/components/motion_strategy/motion_strategy.c \
    $ROOT/components/motion_strategy/motion_strategy_ard1.c \
    $ROOT/components/ard1_core/servo_zero_leg_math.c \
    $ROOT/components/ard1_core/servo_zero_quad_demo.c -lm
gcc $WARN -I$ROOT/components/safety_policy/include -o test_safety_policy.bin \
    test_safety_policy.c $ROOT/components/safety_policy/safety_policy.c
gcc $WARN -Ipca9685_driver_stub \
    -I$ROOT/components/actuator_pca9685/include \
    -I$ROOT/components/leg_fsm/include \
    -DCONFIG_NIULAI_ACTIVE_LEG_FL \
    -o test_pca9685_safety.bin test_pca9685_safety.c
gcc $WARN -I$ROOT/components/actuator_pca9685/include \
    -DCONFIG_NIULAI_FORMAL_CH0_MIN_US=500 \
    -DCONFIG_NIULAI_FORMAL_CH2_MAX_US=2500 \
    -o test_pca9685_safety_rejects_widening.bin \
    test_pca9685_safety_rejects_widening.c
gcc $WARN -I$ROOT/components/actuator_pca9685/include -I$ROOT/main \
    -o test_safety_contracts.bin test_safety_contracts.c
gcc $WARN -Wno-unused-function -ffunction-sections -fdata-sections \
    -DQW_PCA9685_HOST_TEST \
    -Ipca9685_driver_stub -Ipca9685_driver_stub/driver \
    -I$ROOT/components/qw_hal/include \
    -I$ROOT/components/actuator_pca9685/include \
    -I$ROOT/components/motion_strategy/include \
    -I$ROOT/components/leg_fsm/include \
    -o test_pca9685_driver_stop.bin test_pca9685_driver_stop.c \
    $ROOT/components/actuator_pca9685/actuator_pca9685.c \
    $ROOT/components/actuator_pca9685/servo_interpolator.c -Wl,--gc-sections -lm
gcc $WARN $INC -o test_ble_protocol.bin test_ble_protocol.c $PURE_SRCS -lm
gcc $WARN $INC -o test_remote_input_state.bin test_remote_input_state.c $PURE_SRCS -lm
gcc $WARN -I$ROOT/main -o test_display_hud_model.bin test_display_hud_model.c $ROOT/main/display_hud_model.c
gcc $WARN -I$ROOT/main -o test_display_hud_renderer.bin test_display_hud_renderer.c $ROOT/main/display_hud_model.c $ROOT/main/display_hud_renderer.c
gcc $WARN -Idisplay_service_stub -I$ROOT/components/display_service/include \
    -o test_display_service.bin test_display_service.c \
    $ROOT/components/display_service/display_service.c
gcc $WARN -Ist7789_driver_stub -I$ROOT/components/qw_hal/include \
    -I$ROOT/components/qw_hal_espidf/include \
    -I$ROOT/components/st7789_driver/include \
    -o test_st7789_driver.bin test_st7789_driver.c \
    $ROOT/components/st7789_driver/st7789.c
gcc $WARN -I$ROOT/components/ble_input/include -o test_ble_endpoint_mock.bin test_ble_endpoint_mock.c $ROOT/components/ble_input/ble_protocol.c $ROOT/components/ble_input/remote_input_state.c
gcc $WARN -I$ROOT/components/mpu6500_driver/include -o test_mpu6500_math.bin test_mpu6500_math.c $ROOT/components/mpu6500_driver/mpu6500_math.c -lm
gcc $WARN -I$ROOT/main -o test_fall_detector.bin test_fall_detector.c $ROOT/main/fall_detector.c -lm
gcc $WARN $CONTACT_INC -I. -o test_foot_contact_debounce.bin test_foot_contact_debounce.c $ROOT/components/foot_contact/foot_contact_debounce.c
gcc $WARN -Ifoot_contact_stub -I$ROOT/components/foot_contact/include \
    -I$ROOT/components/qw_hal/include -o test_foot_contact.bin \
    test_foot_contact.c $ROOT/components/foot_contact/foot_contact.c \
    $ROOT/components/foot_contact/foot_contact_debounce.c
gcc $WARN -I$ROOT/components/actuator_pca9685/include -o test_servo_interpolator.bin test_servo_interpolator.c $ROOT/components/actuator_pca9685/servo_interpolator.c -lm
gcc $WARN $VOICE_INC -o test_voice_keyword.bin test_voice_keyword.c $ROOT/components/voice_keyword/voice_keyword.c -lm
gcc $WARN -I$ROOT/main -o test_audio_command.bin test_audio_command.c $ROOT/main/audio_command.c
gcc $WARN -I$ROOT/main -o test_i2s_sine_command.bin test_i2s_sine_command.c $ROOT/main/i2s_sine_command.c
gcc -DARM $WARN -Wno-unused-but-set-variable -Wno-unused-variable \
    -Wno-unused-parameter $MP3_INC -o test_mp3_decoder.bin test_mp3_decoder.c \
    $MP3_SRCS
gcc $WARN $HAL_INC -o test_qw_hal.bin test_qw_hal.c qw_hal_fake.c \
    $ROOT/components/qw_hal/qw_hal.c

echo "== running =="
./test_leg_fsm.bin
./test_control_glue.bin
./test_motion_strategy.bin
./test_motion_ard1.bin
./test_safety_policy.bin
./test_pca9685_safety.bin
./test_pca9685_safety_rejects_widening.bin
./test_safety_contracts.bin
./test_pca9685_driver_stop.bin
./test_ble_protocol.bin
./test_remote_input_state.bin
./test_display_hud_model.bin
./test_display_hud_renderer.bin
./test_display_service.bin
./test_st7789_driver.bin
./test_ble_endpoint_mock.bin
./test_mpu6500_math.bin
./test_fall_detector.bin
./test_foot_contact.bin
./test_foot_contact_debounce.bin
./test_servo_interpolator.bin
./test_voice_keyword.bin
./test_audio_command.bin
./test_i2s_sine_command.bin
./test_mp3_decoder.bin "$ROOT/../牛来叫妈妈.mp3"
./test_qw_hal.bin

echo "== purity check (forbidden tokens in pure-C sources) =="
if grep -nE 'esp_|freertos|hal/|sdkconfig' $PURE_SRCS; then
    echo "PURITY FAIL: forbidden tokens found in pure-C sources" >&2
    exit 1
fi
echo "purity OK: zero matches"

if grep -nE 'esp_|freertos|hal/|sdkconfig' "$ROOT/main/display_hud_renderer.c"; then
    echo "HUD RENDERER PURITY FAIL: forbidden tokens found" >&2
    exit 1
fi
echo "HUD renderer purity OK: zero matches"

if grep -nE 'esp_|freertos|hal/|sdkconfig' "$ROOT/components/voice_keyword/voice_keyword.c"; then
    echo "VOICE KEYWORD PURITY FAIL: forbidden tokens found" >&2
    exit 1
fi
echo "voice keyword purity OK: zero matches"

if grep -nE 'esp_|freertos|hal/|sdkconfig' "$ROOT/components/qw_hal/qw_hal.c" "$ROOT/components/qw_hal/include/qw_hal.h"; then
    echo "HAL CORE PURITY FAIL: forbidden platform tokens found" >&2
    exit 1
fi
echo "HAL core purity OK: zero matches"

echo "ALL HOST TESTS PASSED"
