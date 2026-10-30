#include <assert.h>
#include <string.h>

#include "display_service.h"

struct st7789_device {
    st7789_config_t config;
};

static struct st7789_device s_panel;
static size_t s_write_calls;
static size_t s_last_length;
static uint8_t s_first_bytes[2];
static uint16_t s_last_window[4];
static bool s_deinit_pending;

esp_err_t st7789_init(const st7789_config_t *config, st7789_handle_t *out)
{
    s_panel.config = *config;
    *out = &s_panel;
    return ESP_OK;
}

esp_err_t st7789_set_backlight(st7789_handle_t dev, bool on)
{
    (void)dev;
    (void)on;
    return ESP_OK;
}

esp_err_t st7789_set_window(st7789_handle_t dev, uint16_t x0, uint16_t y0,
                            uint16_t x1, uint16_t y1)
{
    (void)dev;
    s_last_window[0] = x0;
    s_last_window[1] = y0;
    s_last_window[2] = x1;
    s_last_window[3] = y1;
    return ESP_OK;
}

esp_err_t st7789_write_pixels(st7789_handle_t dev, const void *rgb565,
                              size_t length_bytes)
{
    (void)dev;
    const uint8_t *bytes = (const uint8_t *)rgb565;
    s_write_calls++;
    s_last_length = length_bytes;
    if (s_write_calls == 1U) {
        s_first_bytes[0] = bytes[0];
        s_first_bytes[1] = bytes[1];
    }
    return ESP_OK;
}

bool st7789_deinit(st7789_handle_t dev)
{
    (void)dev;
    return !s_deinit_pending;
}

static st7789_config_t config(uint16_t width, uint16_t height)
{
    st7789_config_t value = {0};
    value.width = width;
    value.height = height;
    value.madctl = width > height ? 0x60U : 0x00U;
    return value;
}

static void test_portrait(void)
{
    qw_display_service_t service = {0};
    const st7789_config_t panel = config(170U, 320U);
    uint16_t framebuffer[170U * 320U];
    framebuffer[0] = 0x1234;
    memset(&framebuffer[1], 0, sizeof(framebuffer) - sizeof(framebuffer[0]));
    s_write_calls = 0;
    assert(qw_display_service_init(&panel, QW_DISPLAY_PORTRAIT, &service) ==
           ESP_OK);
    assert(qw_display_service_width(&service) == 170U);
    assert(qw_display_service_height(&service) == 320U);
    assert(qw_display_service_present(&service, framebuffer,
                                      sizeof(framebuffer) /
                                          sizeof(framebuffer[0])) == ESP_OK);
    assert(framebuffer[0] == 0x1234);
    assert(s_write_calls == 27U);
    assert(s_first_bytes[0] == 0x12 && s_first_bytes[1] == 0x34);
    assert(s_last_window[0] == 0U && s_last_window[1] == 312U);
    assert(s_last_window[2] == 169U && s_last_window[3] == 319U);
    assert(s_last_length == 8U * 170U * 2U);
    qw_display_service_deinit(&service);
}

static void test_landscape(void)
{
    qw_display_service_t service = {0};
    const st7789_config_t panel = config(320U, 170U);
    uint16_t framebuffer[320U * 170U] = {0};
    assert(qw_display_service_init(&panel, QW_DISPLAY_LANDSCAPE, &service) ==
           ESP_OK);
    assert(qw_display_service_present(&service, framebuffer,
                                      sizeof(framebuffer) /
                                          sizeof(framebuffer[0])) == ESP_OK);
    assert(s_last_window[0] == 0U && s_last_window[1] == 168U);
    assert(s_last_window[2] == 319U && s_last_window[3] == 169U);
    assert(s_last_length == 2U * 320U * 2U);
    qw_display_service_deinit(&service);
}

static void test_pending_deinit_preserves_ownership(void)
{
    qw_display_service_t service = {0};
    const st7789_config_t panel = config(170U, 320U);
    s_deinit_pending = true;
    assert(qw_display_service_init(&panel, QW_DISPLAY_PORTRAIT, &service) ==
           ESP_OK);
    qw_display_service_deinit(&service);
    assert(service.panel != NULL);
    assert(service.transfer != NULL);
    assert(qw_display_service_width(&service) == 170U);
    s_deinit_pending = false;
    qw_display_service_deinit(&service);
    assert(service.panel == NULL);
    assert(service.transfer == NULL);
}

int main(void)
{
    assert(qw_display_service_init(&(st7789_config_t){.width = 320U,
                                                       .height = 320U},
                                   QW_DISPLAY_PORTRAIT,
                                   &(qw_display_service_t){0}) ==
                                   ESP_ERR_INVALID_ARG);
    assert(qw_display_service_init(&(st7789_config_t){.width = 320U,
                                                       .height = 170U,
                                                       .madctl = 0x00U},
                                   QW_DISPLAY_LANDSCAPE,
                                   &(qw_display_service_t){0}) ==
           ESP_ERR_INVALID_ARG);
    test_portrait();
    test_landscape();
    test_pending_deinit_preserves_ownership();
    return 0;
}
