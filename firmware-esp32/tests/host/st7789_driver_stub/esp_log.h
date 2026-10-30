#ifndef QW_ST7789_DRIVER_STUB_ESP_LOG_H
#define QW_ST7789_DRIVER_STUB_ESP_LOG_H

#define ESP_LOGE(tag, format, ...) \
    do {                              \
        (void)(tag);                  \
        (void)(format);               \
    } while (0)

#endif
