#ifndef SERVO_ZERO_TRACE_H
#define SERVO_ZERO_TRACE_H

#include <stdbool.h>
#include <stdint.h>

#define SERVO_ZERO_TRACE_INVALID_ID UINT32_MAX

typedef enum {
    SERVO_ZERO_TRACE_OFF = 0,
    SERVO_ZERO_TRACE_RX_ONLY,
    SERVO_ZERO_TRACE_LOCAL,
    SERVO_ZERO_TRACE_E2E,
} servo_zero_trace_mode_t;

/* Starts a new bounded trace. It does not touch GPIO, PCA9685 or /OE. */
void servo_zero_trace_begin(servo_zero_trace_mode_t mode);
void servo_zero_trace_halt(void);
bool servo_zero_trace_is_active(void);
servo_zero_trace_mode_t servo_zero_trace_mode(void);

/* A command record is created at the parser boundary. */
uint32_t servo_zero_trace_command_rx(uint32_t sequence, uint16_t pulse_us);
void servo_zero_trace_control_accept(uint32_t record_id);

/* Local/control records are created by a fixed-period motion task. */
uint32_t servo_zero_trace_control_tick(void);

/* I2C calls attach to the current command/tick record. */
void servo_zero_trace_set_current(uint32_t record_id);
void servo_zero_trace_i2c_begin(void);
void servo_zero_trace_i2c_end(int32_t result);

/* Emits the ring contents after a test; this is intentionally the only
 * potentially large diagnostic output operation. */
void servo_zero_trace_dump(void);
void servo_zero_trace_dump_page(uint32_t offset, uint32_t requested_count);

#endif /* SERVO_ZERO_TRACE_H */
