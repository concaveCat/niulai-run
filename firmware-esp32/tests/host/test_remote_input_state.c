#include <stdint.h>

#include "qw_check.h"
#include "remote_input_state.h"

static void test_no_timeout_before_first_packet(void)
{
    remote_leg_input_t input;
    qw_remote_input_init(&input);

    CHECK(!input.has_received);
    CHECK(!qw_remote_input_apply_failsafe(&input, 10000000, 500000));
    CHECK(!input.failsafe_active);
    CHECK(!qw_remote_input_is_fresh(&input, 0, 500000));
}

static void test_accept_and_exact_timeout_boundary(void)
{
    remote_leg_input_t input;
    qw_remote_input_init(&input);
    qw_remote_input_accept(&input, 0x34, 0x09, 1000000);

    CHECK(input.has_received);
    CHECK_INT_EQ(input.sequence, 0x34);
    CHECK_INT_EQ(input.leg_mask, 0x09);
    CHECK(qw_remote_input_is_fresh(&input, 1499999, 500000));
    CHECK(!qw_remote_input_apply_failsafe(&input, 1499999, 500000));

    CHECK(!qw_remote_input_is_fresh(&input, 1500000, 500000));
    CHECK(qw_remote_input_apply_failsafe(&input, 1500000, 500000));
    CHECK_INT_EQ(input.leg_mask, 0);
    CHECK(input.failsafe_active);
    CHECK(!qw_remote_input_apply_failsafe(&input, 1600000, 500000));
}

static void test_disconnect_clears_without_synthetic_failsafe(void)
{
    remote_leg_input_t input;
    qw_remote_input_init(&input);
    qw_remote_input_accept(&input, 0x20, 0x0f, 2000000);
    qw_remote_input_clear(&input);

    CHECK_INT_EQ(input.sequence, 0x20);
    CHECK_INT_EQ(input.leg_mask, 0);
    CHECK(!input.has_received);
    CHECK(!input.failsafe_active);
    CHECK(!qw_remote_input_apply_failsafe(&input, 3000000, 500000));
}

static void test_clock_regression_is_not_fresh_or_expired(void)
{
    remote_leg_input_t input;
    qw_remote_input_init(&input);
    qw_remote_input_accept(&input, 1, 1, 2000000);

    CHECK(!qw_remote_input_is_fresh(&input, 1999999, 500000));
    CHECK(!qw_remote_input_apply_failsafe(&input, 1999999, 500000));
    CHECK_INT_EQ(input.leg_mask, 1);
    CHECK(!input.failsafe_active);
}

int main(void)
{
    RUN_TEST(test_no_timeout_before_first_packet);
    RUN_TEST(test_accept_and_exact_timeout_boundary);
    RUN_TEST(test_disconnect_clears_without_synthetic_failsafe);
    RUN_TEST(test_clock_regression_is_not_fresh_or_expired);
    printf("test_remote_input_state: ALL PASS (%d checks)\n", qw_checks_run);
    return 0;
}
