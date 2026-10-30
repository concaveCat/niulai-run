#include "foot_contact.h"
#include "qw_check.h"

#include <stdbool.h>

static int s_levels[49];

qw_hal_gpio_t qw_hal_espidf_gpio(void)
{
    return (qw_hal_gpio_t){0};
}

static qw_hal_status_t configure_input(void *ctx, int pin, bool pull_up)
{
    (void)ctx;
    (void)pull_up;
    return pin >= 0 && pin < 49 ? QW_HAL_OK : QW_HAL_INVALID_ARG;
}

static int read_level(void *ctx, int pin)
{
    (void)ctx;
    return pin >= 0 && pin < 49 ? s_levels[pin] : -1;
}

static void sample_twice(void)
{
    foot_contact_sample();
    foot_contact_sample();
}

static void test_uniform_active_high_polarity(void)
{
    s_levels[39] = 0;
    s_levels[40] = 0;
    s_levels[41] = 0;
    s_levels[42] = 0;
    CHECK(foot_contact_init_with_gpio(&(qw_hal_gpio_t){
        .ctx = NULL,
        .configure_input = configure_input,
        .read = read_level,
    }));

    sample_twice();
    /* Suspended C-NC state: all four legs use active-high semantics. */
    CHECK_INT_EQ(foot_contact_raw_mask(), 0x00);
    CHECK_INT_EQ(foot_contact_mask(), 0x00);

    s_levels[39] = 1;
    s_levels[40] = 1;
    s_levels[41] = 1;
    s_levels[42] = 1;
    sample_twice();
    CHECK_INT_EQ(foot_contact_raw_mask(), 0x0F);
    CHECK_INT_EQ(foot_contact_mask(), 0x0F);
}

static void test_invalid_gpio_level_is_not_contact(void)
{
    s_levels[39] = -1;
    s_levels[40] = -1;
    s_levels[41] = -1;
    s_levels[42] = -1;
    (void)foot_contact_init_with_gpio(&(qw_hal_gpio_t){
        .ctx = NULL,
        .configure_input = configure_input,
        .read = read_level,
    });
    sample_twice();
    CHECK_INT_EQ(foot_contact_raw_mask(), 0x00);
    CHECK_INT_EQ(foot_contact_mask(), 0x00);

    s_levels[39] = 2;
    s_levels[40] = 2;
    s_levels[41] = 2;
    s_levels[42] = 2;
    sample_twice();
    CHECK_INT_EQ(foot_contact_raw_mask(), 0x00);
    CHECK_INT_EQ(foot_contact_gpio_level_mask(), 0x00);
}

static void test_missing_read_callback_is_unavailable(void)
{
    CHECK(!foot_contact_init_with_gpio(&(qw_hal_gpio_t){
        .ctx = NULL,
        .configure_input = configure_input,
        .read = NULL,
    }));
    CHECK(!foot_contact_active_leg_ready());
    CHECK(!foot_contact_all_ready());
}

int main(void)
{
    RUN_TEST(test_uniform_active_high_polarity);
    RUN_TEST(test_invalid_gpio_level_is_not_contact);
    RUN_TEST(test_missing_read_callback_is_unavailable);
    printf("test_foot_contact: ALL PASS (%d checks)\n", qw_checks_run);
    return 0;
}
