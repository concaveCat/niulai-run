#include <assert.h>

#include "pca9685_safety.h"

int main(void)
{
    /* These overrides model an unsafe sdkconfig. The reviewed hard envelope
     * must reject the configuration and the widened pulse itself. */
    assert(!qw_pca9685_limits_valid());
    assert(!qw_pca9685_pulse_within_limit(0U, 1000));
    assert(!qw_pca9685_pulse_within_limit(2U, 2000));
    return 0;
}
