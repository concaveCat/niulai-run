#include "qw_hal.h"

const char *qw_hal_status_name(qw_hal_status_t status)
{
    switch (status) {
    case QW_HAL_OK:
        return "ok";
    case QW_HAL_INVALID_ARG:
        return "invalid_arg";
    case QW_HAL_NOT_READY:
        return "not_ready";
    case QW_HAL_IO_ERROR:
        return "io_error";
    case QW_HAL_TIMEOUT:
        return "timeout";
    default:
        return "unknown";
    }
}
