#include "display_hud_renderer.h"
#include "qw_check.h"

#include <stdint.h>
#include <stdlib.h>

#define RGB565(r, g, b)                                                       \
    ((uint16_t)((((uint16_t)(r) & 0xf8U) << 8) |                             \
                (((uint16_t)(g) & 0xfcU) << 3) | ((uint16_t)(b) >> 3)))

static void test_orientation_dimensions(void)
{
    CHECK_INT_EQ(qw_hud_render_width(QW_HUD_ORIENTATION_PORTRAIT), 170);
    CHECK_INT_EQ(qw_hud_render_height(QW_HUD_ORIENTATION_PORTRAIT), 320);
    CHECK_INT_EQ(qw_hud_render_width(QW_HUD_ORIENTATION_LANDSCAPE), 320);
    CHECK_INT_EQ(qw_hud_render_height(QW_HUD_ORIENTATION_LANDSCAPE), 170);
}

static void test_render_rejects_invalid_buffer_or_state(void)
{
    qw_hud_state_t state = {0};
    uint16_t pixel = 0;
    CHECK(!qw_hud_render(NULL, 1, &state, QW_HUD_ORIENTATION_PORTRAIT));
    CHECK(!qw_hud_render(&pixel, 1, &state, QW_HUD_ORIENTATION_PORTRAIT));
    state.ble = (qw_hud_ble_state_t)99;
    CHECK(!qw_hud_render(&pixel, 1, &state, QW_HUD_ORIENTATION_PORTRAIT));
}

static void test_render_portrait_and_preserve_guards(void)
{
    const size_t pixels = (size_t)QW_HUD_PORTRAIT_WIDTH * QW_HUD_PORTRAIT_HEIGHT;
    uint16_t *storage = malloc((pixels + 2U) * sizeof(*storage));
    qw_hud_state_t state = {
        .ble = QW_HUD_BLE_CONNECTED,
        .safety = QW_HUD_SAFETY_FAILSAFE,
        .uptime_seconds = 83,
        .oe_disabled = true,
        .accel_state = QW_HUD_ACCEL_NORMAL,
        .accel_magnitude_g = 0.12f,
        .legs = {{true, true, false}, {false, false, false},
                 {true, false, true}, {false, true, false}},
    };
    CHECK(storage != NULL);
    storage[0] = 0xa55aU;
    storage[pixels + 1U] = 0x5aa5U;
    CHECK(qw_hud_render(&storage[1], pixels, &state,
                        QW_HUD_ORIENTATION_PORTRAIT));
    CHECK_INT_EQ(storage[0], 0xa55aU);
    CHECK_INT_EQ(storage[pixels + 1U], 0x5aa5U);

    qw_hud_rect_t fl = qw_hud_leg_rect(0);
    CHECK_INT_EQ(storage[1], RGB565(24, 31, 44));
    CHECK_INT_EQ(storage[1U + (size_t)40 * QW_HUD_PORTRAIT_WIDTH + 0U],
                 RGB565(10, 14, 22));
    CHECK_INT_EQ(storage[1U + (size_t)fl.y * QW_HUD_PORTRAIT_WIDTH + fl.x],
                 RGB565(55, 150, 255));
    CHECK_INT_EQ(storage[1U + (size_t)(fl.y + 39U) * QW_HUD_PORTRAIT_WIDTH +
                           fl.x + 42U],
                 RGB565(44, 210, 115));
    CHECK_INT_EQ(storage[1U + (size_t)319 * QW_HUD_PORTRAIT_WIDTH],
                 RGB565(44, 210, 115));
    free(storage);
}

static void test_render_landscape_layout(void)
{
    const size_t pixels = (size_t)QW_HUD_LANDSCAPE_WIDTH * QW_HUD_LANDSCAPE_HEIGHT;
    uint16_t *fb = malloc(pixels * sizeof(*fb));
    qw_hud_state_t state = {
        .ble = QW_HUD_BLE_DISCONNECTED,
        .safety = QW_HUD_SAFETY_SAFE,
        .oe_disabled = true,
        .accel_state = QW_HUD_ACCEL_WARNING,
        .accel_magnitude_g = 0.30f,
    };
    CHECK(fb != NULL);
    CHECK(qw_hud_render(fb, pixels, &state, QW_HUD_ORIENTATION_LANDSCAPE));
    for (unsigned i = 0; i < QW_HUD_LEG_COUNT; i++) {
        qw_hud_rect_t r = qw_hud_landscape_leg_rect(i);
        CHECK_INT_EQ(fb[(size_t)r.y * QW_HUD_LANDSCAPE_WIDTH + r.x],
                     RGB565(100, 112, 128));
    }
    CHECK_INT_EQ(fb[(size_t)169 * QW_HUD_LANDSCAPE_WIDTH],
                 RGB565(255, 190, 45));
    state.accel_state = QW_HUD_ACCEL_HIGH;
    state.accel_magnitude_g = 0.80f;
    CHECK(qw_hud_render(fb, pixels, &state, QW_HUD_ORIENTATION_LANDSCAPE));
    CHECK_INT_EQ(fb[(size_t)169 * QW_HUD_LANDSCAPE_WIDTH],
                 RGB565(245, 70, 75));
    state.accel_state = QW_HUD_ACCEL_INVALID;
    state.accel_magnitude_g = 0.0f;
    CHECK(qw_hud_render(fb, pixels, &state, QW_HUD_ORIENTATION_LANDSCAPE));
    CHECK_INT_EQ(fb[(size_t)169 * QW_HUD_LANDSCAPE_WIDTH],
                 RGB565(100, 112, 128));
    free(fb);
}

static void test_failure_transition_is_full_red_then_black(void)
{
    const size_t pixels = (size_t)QW_HUD_LANDSCAPE_WIDTH * QW_HUD_LANDSCAPE_HEIGHT;
    uint16_t *fb = malloc(pixels * sizeof(*fb));
    CHECK(fb != NULL);
    CHECK(qw_hud_render_failure(fb, pixels, QW_HUD_ORIENTATION_LANDSCAPE, 0));
    CHECK_INT_EQ(fb[0], RGB565(248, 0, 0));
    CHECK(qw_hud_render_failure(fb, pixels, QW_HUD_ORIENTATION_LANDSCAPE, 128));
    CHECK(fb[0] < RGB565(248, 0, 0));
    CHECK(fb[0] > 0U);
    CHECK(qw_hud_render_failure(fb, pixels, QW_HUD_ORIENTATION_LANDSCAPE, 255));
    CHECK_INT_EQ(fb[0], 0);
    CHECK(!qw_hud_render_failure(fb, 1, QW_HUD_ORIENTATION_LANDSCAPE, 0));
    free(fb);
}

int main(void)
{
    RUN_TEST(test_orientation_dimensions);
    RUN_TEST(test_render_rejects_invalid_buffer_or_state);
    RUN_TEST(test_render_portrait_and_preserve_guards);
    RUN_TEST(test_render_landscape_layout);
    RUN_TEST(test_failure_transition_is_full_red_then_black);
    printf("test_display_hud_renderer: ALL PASS (%d checks)\n", qw_checks_run);
    return 0;
}
