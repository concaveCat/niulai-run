idf_component_register(SRCS "mpu6500.c" "mpu6500_math.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver qw_hal_espidf)
