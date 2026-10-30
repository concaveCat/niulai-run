# ble_protocol.c is pure C11 (host-tested); ble_input.c and ble_gatt.c are
# the ESP-specific layers (spinlock state + NimBLE GATT server).
idf_component_register(SRCS "ble_protocol.c"
                            "remote_input_state.c"
                            "ble_input.c"
                            "ble_gatt.c"
                       INCLUDE_DIRS "include"
                       REQUIRES bt esp_timer nvs_flash)
