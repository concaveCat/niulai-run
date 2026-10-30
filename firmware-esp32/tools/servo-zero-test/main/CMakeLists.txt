idf_component_register(
    SRCS "servo_zero_main.c" "servo_zero_math.c"
         "../../../components/ard1_core/servo_zero_leg_math.c"
         "../../../components/ard1_core/servo_zero_quad_demo.c"
         "servo_zero_us_profile.c"
         "servo_zero_trace.c" "servo_uart_protocol.c" "servo_zero_tcp_diag.c"
    INCLUDE_DIRS "." "../../../components/ard1_core/include"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_timer nvs_flash esp_wifi
             esp_netif esp_event
)
