#include "foot_contact_debounce.h"
#include "qw_check.h"

static void test_requires_consecutive_samples(void)
{
    foot_contact_debounce_t state;
    foot_contact_debounce_init(&state, false);
    CHECK(!foot_contact_debounce_update(&state, true, 3));
    CHECK(!foot_contact_debounce_update(&state, false, 3));
    CHECK(!foot_contact_debounce_update(&state, true, 3));
    CHECK(!foot_contact_debounce_update(&state, true, 3));
    CHECK(foot_contact_debounce_update(&state, true, 3));
}

static void test_release_is_debounced_too(void)
{
    foot_contact_debounce_t state;
    foot_contact_debounce_init(&state, true);
    CHECK(foot_contact_debounce_update(&state, false, 2));
    CHECK(!foot_contact_debounce_update(&state, false, 2));
    CHECK(!foot_contact_debounce_update(&state, true, 2));
    CHECK(foot_contact_debounce_update(&state, true, 2));
}

static void test_threshold_one_is_immediate(void)
{
    foot_contact_debounce_t state;
    foot_contact_debounce_init(&state, false);
    CHECK(foot_contact_debounce_update(&state, true, 1));
    CHECK(!foot_contact_debounce_update(&state, false, 1));
}

int main(void)
{
    RUN_TEST(test_requires_consecutive_samples);
    RUN_TEST(test_release_is_debounced_too);
    RUN_TEST(test_threshold_one_is_immediate);
    printf("test_foot_contact_debounce: ALL PASS (%d checks)\n", qw_checks_run);
    return 0;
}
