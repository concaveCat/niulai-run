set(NIULAI_MAIN_SRCS
    "app_main.c"
    "foot_contact_test.c"
    "gpio39_cli.c"
    "display_test.c"
    "display_hud.c"
    "display_hud_assets.c"
    "display_hud_renderer.c"
    "display_hud_model.c"
    "audio_test.c"
    "control_task.c"
    "watchdog_task.c"
    "safety_gate.c"
    "runtime_i2s.c"
    "fall_detector.c"
    "runtime_imu.c")

if(NOT CONFIG_NIULAI_I2S_SINE_TEST AND
   NOT CONFIG_NIULAI_I2S_LOOPBACK_TEST AND
   NOT CONFIG_NIULAI_MPU6500_PROBE_TEST)
    # The voice profile owns the I2S clock lines while it listens and pauses
    # RX before invoking the blocking MP3 path. Keep the player in the same
    # image so the keyword action has a real, exclusive TX implementation.
    list(APPEND NIULAI_MAIN_SRCS "audio_command.c" "audio_player.c"
         "voice_keyword_test.c")
endif()

if(CONFIG_NIULAI_I2S_SINE_TEST)
    list(APPEND NIULAI_MAIN_SRCS "i2s_sine_test.c" "i2s_sine_command.c")
endif()

if(CONFIG_NIULAI_I2S_LOOPBACK_TEST)
    list(APPEND NIULAI_MAIN_SRCS "i2s_loopback_test.c")
endif()

if(CONFIG_NIULAI_MPU6500_PROBE_TEST)
    list(APPEND NIULAI_MAIN_SRCS "mpu6500_probe_test.c")
endif()

set(NIULAI_MAIN_REQUIRES
    ble_input
    foot_contact
    actuator_pca9685
    control_glue
    motion_strategy
    safety_policy
    st7789_driver
    display_service
    i2s_audio_driver
    voice_keyword
    mpu6500_driver
    i2c_bus_service
    qw_hal_espidf
    driver
    esp_timer
    nvs_flash)

if(NOT CONFIG_NIULAI_I2S_SINE_TEST AND
   NOT CONFIG_NIULAI_I2S_LOOPBACK_TEST AND
   NOT CONFIG_NIULAI_MPU6500_PROBE_TEST)
    list(APPEND NIULAI_MAIN_REQUIRES mp3_decoder)
endif()

idf_component_register(SRCS ${NIULAI_MAIN_SRCS}
                       INCLUDE_DIRS "."
                       REQUIRES ${NIULAI_MAIN_REQUIRES})

set(QW_HUD_ASSET_DIR "${CMAKE_CURRENT_LIST_DIR}/assets")
target_add_binary_data(${COMPONENT_LIB} "${QW_HUD_ASSET_DIR}/waiting_for_ble.rgb565" BINARY
                       RENAME_TO qw_hud_waiting_rgb565)
target_add_binary_data(${COMPONENT_LIB} "${QW_HUD_ASSET_DIR}/walking_solo.rgb565" BINARY
                       RENAME_TO qw_hud_walking_solo_rgb565)
target_add_binary_data(${COMPONENT_LIB} "${QW_HUD_ASSET_DIR}/walking_multi.rgb565" BINARY
                       RENAME_TO qw_hud_walking_multi_rgb565)

if(NOT CONFIG_NIULAI_I2S_SINE_TEST AND
   NOT CONFIG_NIULAI_I2S_LOOPBACK_TEST AND
   NOT CONFIG_NIULAI_MPU6500_PROBE_TEST)
    set(QW_MAMA_MP3 "${CMAKE_CURRENT_LIST_DIR}/../../牛来叫妈妈.mp3")
    if(NOT EXISTS "${QW_MAMA_MP3}")
        message(FATAL_ERROR
                "Missing embedded audio asset: ${QW_MAMA_MP3}")
    endif()
    set(QW_MAMA_MP3_EMBED "${CMAKE_CURRENT_BINARY_DIR}/qw_cow_mama.mp3")
    # Keep the generated assembly/dependency filenames ASCII. This avoids a
    # Ninja depfile encoding error while still tracking the requested source file.
    configure_file("${QW_MAMA_MP3}" "${QW_MAMA_MP3_EMBED}" COPYONLY)
    target_add_binary_data(${COMPONENT_LIB} "${QW_MAMA_MP3_EMBED}" BINARY
                           RENAME_TO qw_cow_mama_mp3)
endif()
