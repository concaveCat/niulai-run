# Pure C keyword-spotting core.  The ESP32 service layer lives in main/ so
# this component remains host-testable and replaceable by an ESP-SR backend.
idf_component_register(SRCS "voice_keyword.c"
                       INCLUDE_DIRS "include")
