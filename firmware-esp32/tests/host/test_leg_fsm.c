/* Host tests for the pure-C leg FSM (components/leg_fsm).
 *
 * Mirrors firmware/tests/test_leg_fsm.py 1:1 (case names reference the
 * Python test they mirror) plus the tick-ordering edge cases from the plan.
 * The Python test for non-bool inputs ("not-a-button") has no C equivalent:
 * leg_fsm_update takes C bool parameters, so that sanitize branch cannot be
 * triggered from a typed caller; the dt sanitize branch (dt = -1.0, NaN) is
 * exercised here instead (see leg_fsm.h file header).
 */
#include <math.h>
#include <string.h>

#include "qw_check.h"
#include "leg_fsm.h"

static void press_and_reach_swing(leg_fsm_t *fsm)
{
    /* mirrors LegFsmTests.press_and_reach_swing */
    leg_fsm_update(fsm, true, false, 0.0f);
    leg_fsm_update(fsm, true, false, fsm->config.lift_duration_s);
    CHECK(fsm->state == LEG_STATE_SWING);
}

/* mirrors Python test_normal_press_hold_release_contact_flow */
static void test_normal_press_hold_release_contact_flow(void)
{
    leg_fsm_t fsm;
    leg_fsm_config_t cfg = {0.1f, 2.0f, 0.5f, 1.0f, 0.0f};
    CHECK(leg_fsm_init(&fsm, &cfg));

    press_and_reach_swing(&fsm);

    leg_command_t cmd = leg_fsm_update(&fsm, true, false, 0.25f);
    CHECK(cmd.mode == LEG_CMD_SWING);
    CHECK_FLOAT_NEAR(cmd.progress, 0.5f, 1e-6f);

    cmd = leg_fsm_update(&fsm, false, false, 0.01f);
    CHECK(cmd.mode == LEG_CMD_SEARCH);

    cmd = leg_fsm_update(&fsm, false, true, 0.01f);
    CHECK(cmd.mode == LEG_CMD_HOLD_SUPPORT);
    CHECK(fsm.state == LEG_STATE_SUPPORT);
}

static bool known_reason(const char *reason)
{
    static const char *const known[] = {
        "button press", "lift complete", "button release",
        "foot contact", "state timeout", "invalid input ignored",
    };
    for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
        if (strcmp(reason, known[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* mirrors Python test_long_hold_is_capped_and_does_not_plan_another_leg
 * (the Python repr-grep for "gait" becomes a reason-whitelist check) */
static void test_long_hold_is_capped_and_does_not_plan_another_leg(void)
{
    leg_fsm_t fsm;
    leg_fsm_config_t cfg = {0.1f, 1.0f, 0.5f, 0.3f, 0.0f};
    CHECK(leg_fsm_init(&fsm, &cfg));

    press_and_reach_swing(&fsm);
    leg_fsm_update(&fsm, true, false, 10.0f);
    CHECK(fsm.state == LEG_STATE_SWING);
    CHECK_FLOAT_NEAR(fsm.swing_progress, 0.3f, 1e-6f);

    uint32_t n = leg_fsm_transition_count(&fsm);
    for (uint32_t i = 0; i < n; i++) {
        const leg_transition_t *t = leg_fsm_transition_get(&fsm, i);
        CHECK(t != NULL);
        CHECK(known_reason(t->reason)); /* no gait/auto-planning artifacts */
    }
}

/* mirrors Python test_release_without_contact_remains_in_search */
static void test_release_without_contact_remains_in_search(void)
{
    leg_fsm_t fsm;
    leg_fsm_config_t cfg = {0.1f, 1.0f, 0.5f, 1.0f, 0.0f};
    CHECK(leg_fsm_init(&fsm, &cfg));

    press_and_reach_swing(&fsm);
    leg_fsm_update(&fsm, false, false, 0.1f);
    leg_fsm_update(&fsm, false, false, 1.0f);
    CHECK(fsm.state == LEG_STATE_SEARCH);
}

/* mirrors Python test_state_timeout_enters_safe_fault */
static void test_state_timeout_enters_safe_fault(void)
{
    leg_fsm_t fsm;
    leg_fsm_config_t cfg = {5.0f, 1.0f, 0.5f, 1.0f, 0.5f};
    CHECK(leg_fsm_init(&fsm, &cfg));

    leg_fsm_update(&fsm, true, false, 0.0f);
    leg_fsm_update(&fsm, true, false, 0.6f);
    CHECK(fsm.state == LEG_STATE_FAULT);
    CHECK(leg_fsm_update(&fsm, true, false, 0.0f).mode == LEG_CMD_SAFE_STOP);
}

/* mirrors Python test_invalid_input_is_logged_without_crashing
 * (Python passes a non-bool pressed AND dt = -1.0 and sees 2 transitions;
 * C cannot express the non-bool branch, so only the dt branch is exercised
 * -> 1 transition. NaN extends the same branch.) */
static void test_invalid_input_is_logged_without_crashing(void)
{
    leg_fsm_t fsm;
    leg_fsm_config_t cfg = {0.2f, 1.0f, 0.5f, 1.0f, 0.0f};
    CHECK(leg_fsm_init(&fsm, &cfg));

    leg_command_t cmd = leg_fsm_update(&fsm, false, false, -1.0f);
    CHECK(cmd.mode == LEG_CMD_HOLD_SUPPORT);
    CHECK(fsm.state == LEG_STATE_SUPPORT);
    CHECK_INT_EQ(leg_fsm_transition_count(&fsm), 1);
    CHECK_STR_EQ(leg_fsm_transition_get(&fsm, 0)->reason,
                 "invalid input ignored");
    CHECK(fsm.state_elapsed_s == 0.0f); /* dt sanitized to 0.0 */

    leg_fsm_update(&fsm, false, false, NAN);
    CHECK_INT_EQ(leg_fsm_transition_count(&fsm), 2);
    CHECK_STR_EQ(leg_fsm_transition_get(&fsm, 1)->reason,
                 "invalid input ignored");
    CHECK(fsm.state == LEG_STATE_SUPPORT);
}

/* tick-ordering edge: in LIFT the state timeout (strictly >) is checked
 * BEFORE lift complete (>=), so a dt that satisfies both must fault */
static void test_lift_timeout_checked_before_lift_complete(void)
{
    leg_fsm_t fsm;
    leg_fsm_config_t cfg = {0.1f, 1.0f, 0.5f, 1.0f, 0.5f};
    CHECK(leg_fsm_init(&fsm, &cfg));

    leg_fsm_update(&fsm, true, false, 0.0f); /* -> LIFT */
    leg_fsm_update(&fsm, true, false, 0.6f); /* 0.6 > 0.5 AND 0.6 >= 0.1 */
    CHECK(fsm.state == LEG_STATE_FAULT);
    const leg_transition_t *t = leg_fsm_transition_get(&fsm, 1);
    CHECK_STR_EQ(t->reason, "state timeout");
}

/* tick-ordering edge: in SWING the release branch runs BEFORE progress
 * accumulation and BEFORE the timeout check, and the release tick must not
 * accumulate progress */
static void test_swing_release_before_timeout_no_progress_on_release_tick(void)
{
    leg_fsm_t fsm;
    leg_fsm_config_t cfg = {0.1f, 2.0f, 0.5f, 1.0f, 0.3f};
    CHECK(leg_fsm_init(&fsm, &cfg));

    press_and_reach_swing(&fsm);
    leg_fsm_update(&fsm, true, false, 0.1f); /* progress 0.2, no timeout */
    CHECK_FLOAT_NEAR(fsm.swing_progress, 0.2f, 1e-6f);

    /* dt 0.4 would both accumulate progress and trip the timeout if the
     * release branch did not run first */
    leg_command_t cmd = leg_fsm_update(&fsm, false, false, 0.4f);
    CHECK(cmd.mode == LEG_CMD_SEARCH);
    CHECK(fsm.state == LEG_STATE_SEARCH);
    CHECK_FLOAT_NEAR(fsm.swing_progress, 0.2f, 1e-6f);
    const leg_transition_t *t = leg_fsm_transition_get(&fsm, 2);
    CHECK_STR_EQ(t->reason, "button release");
    CHECK(fsm.state_elapsed_s == 0.0f);
}

/* tick-ordering edge: lift complete uses >= so dt exactly equal to the
 * lift duration completes on the boundary tick */
static void test_lift_complete_ge_boundary(void)
{
    leg_fsm_t fsm;
    leg_fsm_config_t cfg = {0.1f, 1.0f, 0.5f, 1.0f, 0.0f};
    CHECK(leg_fsm_init(&fsm, &cfg));

    leg_fsm_update(&fsm, true, false, 0.0f); /* -> LIFT */
    leg_fsm_update(&fsm, true, false, 0.1f); /* elapsed == duration */
    CHECK(fsm.state == LEG_STATE_SWING);
    const leg_transition_t *t = leg_fsm_transition_get(&fsm, 1);
    CHECK_STR_EQ(t->reason, "lift complete");
}

/* tick-ordering edge: in SWING progress accumulates BEFORE the timeout
 * check, so the faulting tick still contributes progress */
static void test_swing_progress_accumulates_before_timeout_check(void)
{
    leg_fsm_t fsm;
    leg_fsm_config_t cfg = {0.1f, 1.0f, 0.5f, 1.0f, 0.2f};
    CHECK(leg_fsm_init(&fsm, &cfg));

    press_and_reach_swing(&fsm);
    leg_fsm_update(&fsm, true, false, 0.1f); /* progress 0.1, no timeout */
    leg_fsm_update(&fsm, true, false, 0.15f); /* progress first, then fault */
    CHECK(fsm.state == LEG_STATE_FAULT);
    CHECK_FLOAT_NEAR(fsm.swing_progress, 0.25f, 1e-6f);
    const leg_transition_t *t = leg_fsm_transition_get(&fsm, 2);
    CHECK_STR_EQ(t->reason, "state timeout");
}

static void test_config_validation_rejects_invalid_params(void)
{
    leg_fsm_t fsm;
    leg_fsm_config_t cfg = {0.2f, 1.0f, 0.5f, 1.0f, 0.0f};

    CHECK(leg_fsm_init(&fsm, &cfg)); /* valid baseline */

    cfg.lift_duration_s = -0.1f;
    CHECK(!leg_fsm_init(&fsm, &cfg));
    cfg.lift_duration_s = 0.2f;

    cfg.swing_speed = NAN;
    CHECK(!leg_fsm_init(&fsm, &cfg));
    cfg.swing_speed = 1.0f;

    cfg.search_speed = -0.5f;
    CHECK(!leg_fsm_init(&fsm, &cfg));
    cfg.search_speed = 0.5f;

    cfg.max_swing = INFINITY;
    CHECK(!leg_fsm_init(&fsm, &cfg));
    cfg.max_swing = 1.0f;

    CHECK(!leg_fsm_init(NULL, &cfg));
    CHECK(!leg_fsm_init(&fsm, NULL));
}

static void test_config_timeout_disabled_or_positive(void)
{
    leg_fsm_t fsm;
    leg_fsm_config_t cfg = {0.2f, 1.0f, 0.5f, 1.0f, 0.0f};

    cfg.state_timeout_s = 0.0f;
    CHECK(leg_fsm_init(&fsm, &cfg)); /* disabled */

    cfg.state_timeout_s = -1.0f;
    CHECK(leg_fsm_init(&fsm, &cfg)); /* <= 0 means disabled (C convention) */

    cfg.state_timeout_s = 0.5f;
    CHECK(leg_fsm_init(&fsm, &cfg)); /* positive */

    cfg.state_timeout_s = NAN;
    CHECK(!leg_fsm_init(&fsm, &cfg));

    cfg.state_timeout_s = INFINITY;
    CHECK(!leg_fsm_init(&fsm, &cfg));
}

/* reset() must keep the transition ring and transition_total */
static void test_reset_keeps_transition_history(void)
{
    leg_fsm_t fsm;
    leg_fsm_config_t cfg = {0.2f, 1.0f, 0.5f, 1.0f, 0.0f};
    CHECK(leg_fsm_init(&fsm, &cfg));

    leg_fsm_update(&fsm, true, false, 0.0f);        /* "button press" */
    leg_fsm_update(&fsm, false, false, -1.0f);      /* "invalid input ignored" */
    CHECK_INT_EQ(leg_fsm_transition_count(&fsm), 2);

    leg_fsm_reset(&fsm);
    CHECK(fsm.state == LEG_STATE_SUPPORT);
    CHECK(fsm.state_elapsed_s == 0.0f);
    CHECK(fsm.swing_progress == 0.0f);
    CHECK(!fsm.previous_pressed);
    CHECK_INT_EQ(fsm.transition_total, 2); /* history preserved */
    CHECK_INT_EQ(leg_fsm_transition_count(&fsm), 2);
    CHECK_STR_EQ(leg_fsm_transition_get(&fsm, 0)->reason, "button press");
    CHECK_STR_EQ(leg_fsm_transition_get(&fsm, 1)->reason,
                 "invalid input ignored");

    /* the reset FSM responds to a fresh press */
    leg_fsm_update(&fsm, true, false, 0.0f);
    CHECK(fsm.state == LEG_STATE_LIFT);
    CHECK_INT_EQ(fsm.transition_total, 3);
}

/* ring wraps at 64: the oldest entries are overwritten, transition_total
 * stays monotonic, and index 0 remains the oldest stored entry */
static void test_transition_ring_wraps_at_64(void)
{
    leg_fsm_t fsm;
    leg_fsm_config_t cfg = {0.1f, 1.0f, 0.5f, 1.0f, 0.0f};
    CHECK(leg_fsm_init(&fsm, &cfg));

    /* one cycle = press -> lift complete -> release -> contact (4 entries) */
    for (int cycle = 0; cycle < 17; cycle++) {
        leg_fsm_update(&fsm, true, false, 0.0f);
        leg_fsm_update(&fsm, true, false, 0.1f);
        leg_fsm_update(&fsm, false, false, 0.0f);
        leg_fsm_update(&fsm, false, true, 0.0f);
    }

    CHECK_INT_EQ(fsm.transition_total, 68);
    CHECK_INT_EQ(leg_fsm_transition_count(&fsm), 64);

    /* oldest stored is transition #4: cycle 2's "button press" */
    const leg_transition_t *oldest = leg_fsm_transition_get(&fsm, 0);
    CHECK(oldest != NULL);
    CHECK(oldest->from_state == LEG_STATE_SUPPORT);
    CHECK(oldest->to_state == LEG_STATE_LIFT);
    CHECK_STR_EQ(oldest->reason, "button press");

    /* newest stored is transition #67: cycle 17's "foot contact" */
    const leg_transition_t *newest = leg_fsm_transition_get(&fsm, 63);
    CHECK(newest != NULL);
    CHECK(newest->from_state == LEG_STATE_SEARCH);
    CHECK(newest->to_state == LEG_STATE_SUPPORT);
    CHECK_STR_EQ(newest->reason, "foot contact");

    /* entry #3 (cycle 1's "foot contact") has been overwritten: the ring
     * now starts at transition #4 and continues in order */
    const leg_transition_t *second = leg_fsm_transition_get(&fsm, 1);
    CHECK(second != NULL);
    CHECK(second->from_state == LEG_STATE_LIFT);
    CHECK(second->to_state == LEG_STATE_SWING);
    CHECK_STR_EQ(second->reason, "lift complete");

    /* out-of-range access returns NULL */
    CHECK(leg_fsm_transition_get(&fsm, 64) == NULL);

    /* fresh FSM has no transitions */
    leg_fsm_t empty;
    CHECK(leg_fsm_init(&empty, &cfg));
    CHECK_INT_EQ(leg_fsm_transition_count(&empty), 0);
    CHECK(leg_fsm_transition_get(&empty, 0) == NULL);
}

int main(void)
{
    RUN_TEST(test_normal_press_hold_release_contact_flow);
    RUN_TEST(test_long_hold_is_capped_and_does_not_plan_another_leg);
    RUN_TEST(test_release_without_contact_remains_in_search);
    RUN_TEST(test_state_timeout_enters_safe_fault);
    RUN_TEST(test_invalid_input_is_logged_without_crashing);
    RUN_TEST(test_lift_timeout_checked_before_lift_complete);
    RUN_TEST(test_swing_release_before_timeout_no_progress_on_release_tick);
    RUN_TEST(test_lift_complete_ge_boundary);
    RUN_TEST(test_swing_progress_accumulates_before_timeout_check);
    RUN_TEST(test_config_validation_rejects_invalid_params);
    RUN_TEST(test_config_timeout_disabled_or_positive);
    RUN_TEST(test_reset_keeps_transition_history);
    RUN_TEST(test_transition_ring_wraps_at_64);
    printf("test_leg_fsm: ALL PASS (%d checks)\n", qw_checks_run);
    return 0;
}
