idf_component_register(SRCS "actuator_null.c"
                            "actuator_pca9685.c"
                            "servo_interpolator.c"
                       INCLUDE_DIRS "include"
                         REQUIRES driver qw_hal_espidf motion_strategy)
