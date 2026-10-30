#include "motion_strategy.h"

#include <stddef.h>

bool qw_motion_strategy_valid(const qw_motion_strategy_t *strategy)
{
    return strategy != NULL && strategy->ctx != NULL &&
           strategy->init != NULL && strategy->tick != NULL &&
           strategy->snapshot != NULL && strategy->reset != NULL;
}

bool qw_motion_strategy_init(qw_motion_strategy_t *strategy,
                             const qw_motion_init_args_t *args)
{
    return qw_motion_strategy_valid(strategy) && args != NULL &&
           strategy->init(strategy->ctx, args);
}

void qw_motion_strategy_tick(qw_motion_strategy_t *strategy, float dt_s)
{
    if (qw_motion_strategy_valid(strategy)) {
        strategy->tick(strategy->ctx, dt_s);
    }
}

void qw_motion_strategy_commit(qw_motion_strategy_t *strategy, bool applied)
{
    if (qw_motion_strategy_valid(strategy) && strategy->commit != NULL) {
        strategy->commit(strategy->ctx, applied);
    }
}

bool qw_motion_strategy_snapshot(const qw_motion_strategy_t *strategy,
                                 qw_motion_snapshot_t *out)
{
    return qw_motion_strategy_valid(strategy) && out != NULL &&
           strategy->snapshot(strategy->ctx, out);
}

void qw_motion_strategy_reset(qw_motion_strategy_t *strategy)
{
    if (qw_motion_strategy_valid(strategy)) {
        strategy->reset(strategy->ctx);
    }
}
