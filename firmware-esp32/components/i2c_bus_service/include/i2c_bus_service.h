#ifndef QW_I2C_BUS_SERVICE_H
#define QW_I2C_BUS_SERVICE_H

#include <stdbool.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

/* One owner for the carrier I2C0 bus. Device drivers add their own handles
 * to this bus; this service never knows device-specific register semantics. */
esp_err_t qw_i2c_bus_service_init(void);
i2c_master_bus_handle_t qw_i2c_bus_service_bus(void);
bool qw_i2c_bus_service_ready(void);

#endif /* QW_I2C_BUS_SERVICE_H */
