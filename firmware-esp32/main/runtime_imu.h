#ifndef QW_RUNTIME_IMU_H
#define QW_RUNTIME_IMU_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

/* Starts telemetry-only MPU6500 sampling on the shared I2C bus. The service
 * calibrates a short static gravity baseline and publishes a scalar dynamic
 * acceleration indicator with gravity removed; it never feeds a balance or
 * motion-control loop. */
esp_err_t qw_runtime_imu_start(i2c_master_bus_handle_t bus);
bool qw_runtime_imu_ready(void);

typedef struct {
    bool valid;
    bool clipped;
    float accel_magnitude_g;
    float roll_deg;
    float pitch_deg;
    bool fall_detected;
    uint32_t age_ms;
} qw_runtime_imu_snapshot_t;

bool qw_runtime_imu_snapshot(qw_runtime_imu_snapshot_t *out);
bool qw_runtime_imu_fall_detected(void);

#endif /* QW_RUNTIME_IMU_H */
