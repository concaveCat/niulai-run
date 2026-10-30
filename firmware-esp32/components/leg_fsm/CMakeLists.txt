# Pure C11 component: absolutely no ESP-IDF / FreeRTOS dependencies.
# Deliberately no REQUIRES beyond build-system defaults; the host test
# script (tests/host/run_host_tests.sh) greps the sources to enforce this.
idf_component_register(SRCS "leg_fsm.c"
                       INCLUDE_DIRS "include")
