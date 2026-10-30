#include "display_hud_model.h"
#include "qw_check.h"

static void test_layout_contract(void)
{
    qw_hud_rect_t fl = qw_hud_leg_rect(0);
    qw_hud_rect_t fr = qw_hud_leg_rect(1);
    qw_hud_rect_t rl = qw_hud_leg_rect(2);
    qw_hud_rect_t invalid = qw_hud_leg_rect(4);
    CHECK_INT_EQ(QW_HUD_STATUS_BAR_HEIGHT, 28);
    CHECK_INT_EQ(QW_HUD_SAFETY_BAR_HEIGHT, 36);
    CHECK(fl.x < fr.x);
    CHECK(fl.y == fr.y);
    CHECK(rl.y > fl.y);
    CHECK(fr.x + fr.width <= QW_HUD_WIDTH);
    CHECK(rl.y + rl.height < QW_HUD_HEIGHT - QW_HUD_SAFETY_BAR_HEIGHT);
    CHECK_INT_EQ(invalid.width, 0);
}

static void test_landscape_layout_contract(void)
{
    qw_hud_rect_t legs[QW_HUD_LEG_COUNT];
    for (unsigned i = 0; i < QW_HUD_LEG_COUNT; i++) {
        legs[i] = qw_hud_landscape_leg_rect(i);
        CHECK(legs[i].y >= QW_HUD_STATUS_BAR_HEIGHT);
        CHECK(legs[i].x + legs[i].width <= QW_HUD_LANDSCAPE_WIDTH);
        CHECK(legs[i].y + legs[i].height <=
              QW_HUD_LANDSCAPE_HEIGHT - QW_HUD_SAFETY_BAR_HEIGHT);
    }
    CHECK(legs[0].x < legs[1].x);
    CHECK(legs[1].x < legs[2].x);
    CHECK(legs[2].x < legs[3].x);
    CHECK(legs[0].y == legs[3].y);
    CHECK_INT_EQ(legs[1].x - legs[0].x, 80);
    CHECK_INT_EQ(qw_hud_landscape_leg_rect(QW_HUD_LEG_COUNT + 1U).width, 0);
}

static void test_state_and_labels(void)
{
    qw_hud_state_t state = {.ble = QW_HUD_BLE_CONNECTED,
                            .safety = QW_HUD_SAFETY_SAFE,
                            .uptime_seconds = 3661,
                            .oe_disabled = true,
                            .accel_state = QW_HUD_ACCEL_NORMAL,
                            .accel_magnitude_g = 0.98f};
    CHECK(qw_hud_state_valid(&state));
    CHECK_STR_EQ(qw_hud_ble_text(state.ble), "OK");
    CHECK_STR_EQ(qw_hud_safety_text(state.safety), "SAFE");
    char uptime[8];
    qw_hud_format_uptime(state.uptime_seconds, uptime, sizeof(uptime));
    CHECK_STR_EQ(uptime, "01:01");
    state.uptime_seconds = 359999;
    qw_hud_format_uptime(state.uptime_seconds, uptime, sizeof(uptime));
    CHECK_STR_EQ(uptime, "99:59");
    CHECK_STR_EQ(qw_hud_ble_text((qw_hud_ble_state_t)99), "?");
    CHECK(!qw_hud_is_waiting_for_link(&state));
    state.game_mode = QW_HUD_GAME_MODE_MULTI;
    CHECK(qw_hud_state_valid(&state));
    state.game_mode = (qw_hud_game_mode_t)99;
    CHECK(!qw_hud_state_valid(&state));
    state.game_mode = QW_HUD_GAME_MODE_SOLO;
    state.ble = QW_HUD_BLE_DISCONNECTED;
    CHECK(qw_hud_is_waiting_for_link(&state));
}

static void test_gameplay_paw_layout(void)
{
    qw_hud_rect_t fl = qw_hud_paw_rect(0);
    qw_hud_rect_t fr = qw_hud_paw_rect(1);
    qw_hud_rect_t rl = qw_hud_paw_rect(2);
    qw_hud_rect_t rr = qw_hud_paw_rect(3);
    CHECK(fl.x < fr.x);
    CHECK(rl.x < rr.x);
    CHECK(fl.y < rl.y);
    CHECK(fr.y < rr.y);
    CHECK(fl.x + fl.width <= QW_HUD_LANDSCAPE_WIDTH);
    CHECK(fr.x + fr.width <= QW_HUD_LANDSCAPE_WIDTH);
    CHECK(rl.y + rl.height <= QW_HUD_LANDSCAPE_HEIGHT);
    CHECK(rr.y + rr.height <= QW_HUD_LANDSCAPE_HEIGHT);
    CHECK_INT_EQ(fl.width, 40);
    CHECK_INT_EQ(fl.height, 42);
    CHECK_INT_EQ(qw_hud_paw_rect(4).width, 0);
}

static void test_accel_contract(void)
{
    CHECK(qw_hud_accel_classify(0.249f, true, false, 500U) ==
          QW_HUD_ACCEL_NORMAL);
    CHECK(qw_hud_accel_classify(0.250f, true, false, 500U) ==
          QW_HUD_ACCEL_WARNING);
    CHECK(qw_hud_accel_classify(0.599f, true, false, 500U) ==
          QW_HUD_ACCEL_WARNING);
    CHECK(qw_hud_accel_classify(0.600f, true, false, 500U) ==
          QW_HUD_ACCEL_HIGH);
    CHECK(qw_hud_accel_classify(0.24f, true, false, 501U) ==
          QW_HUD_ACCEL_INVALID);
    CHECK(qw_hud_accel_classify(0.24f, true, true, 0U) ==
          QW_HUD_ACCEL_INVALID);
    CHECK(qw_hud_accel_classify(0.24f, false, false, 0U) ==
          QW_HUD_ACCEL_INVALID);
    char text[16];
    qw_hud_format_accel(QW_HUD_ACCEL_NORMAL, 0.98f, text, sizeof(text));
    CHECK_STR_EQ(text, "ACC 0.98G");
    qw_hud_format_accel(QW_HUD_ACCEL_INVALID, 0.0f, text, sizeof(text));
    CHECK_STR_EQ(text, "ACC --");
}

int main(void)
{
    RUN_TEST(test_layout_contract);
    RUN_TEST(test_landscape_layout_contract);
    RUN_TEST(test_state_and_labels);
    RUN_TEST(test_gameplay_paw_layout);
    RUN_TEST(test_accel_contract);
    printf("test_display_hud_model: ALL PASS (%d checks)\n", qw_checks_run);
    return 0;
}
