idf_component_register(SRCS "motion_strategy.c" "motion_strategy_leg_fsm.c"
                              "motion_strategy_ard1.c"
                       INCLUDE_DIRS "include"
                       REQUIRES ard1_core control_glue leg_fsm)
