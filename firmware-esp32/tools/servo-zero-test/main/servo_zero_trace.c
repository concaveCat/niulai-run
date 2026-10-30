#include "servo_zero_trace.h"

#include <stdio.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#define SERVO_ZERO_TRACE_CAPACITY 2048u
#define SERVO_ZERO_TRACE_KIND_COMMAND 1u
#define SERVO_ZERO_TRACE_KIND_TICK 2u

typedef struct {
    uint32_t id;
    uint32_t sequence;
    uint16_t pulse_us;
    uint8_t kind;
    int64_t esp_rx_us;
    int64_t ctrl_accept_us;
    int64_t i2c_begin_us;
    int64_t i2c_end_us;
    int64_t rx_interval_us;
    int64_t tick_interval_us;
    int32_t i2c_result;
} servo_zero_trace_record_t;

static servo_zero_trace_record_t s_records[SERVO_ZERO_TRACE_CAPACITY];
static portMUX_TYPE s_trace_lock = portMUX_INITIALIZER_UNLOCKED;
static servo_zero_trace_mode_t s_mode;
static uint32_t s_next_id;
static uint32_t s_write_index;
static uint32_t s_record_count;
static uint32_t s_dropped_count;
static uint32_t s_current_id = SERVO_ZERO_TRACE_INVALID_ID;
static int64_t s_last_rx_us;
static int64_t s_last_tick_us;
static uint32_t s_i2c_count;
static uint32_t s_i2c_error_count;
static uint32_t s_i2c_interval_count;
static int64_t s_i2c_duration_sum_us;
static int64_t s_i2c_duration_max_us;
static int64_t s_i2c_interval_sum_us;
static int64_t s_i2c_interval_max_us;
static int64_t s_last_i2c_begin_us;

static servo_zero_trace_record_t *find_record_locked(uint32_t id)
{
    for (uint32_t index = 0; index < s_record_count; ++index) {
        const uint32_t slot =
            (s_write_index + SERVO_ZERO_TRACE_CAPACITY - s_record_count +
             index) % SERVO_ZERO_TRACE_CAPACITY;
        if (s_records[slot].id == id) {
            return &s_records[slot];
        }
    }
    return NULL;
}

static uint32_t append_record(uint8_t kind, uint32_t sequence,
                              uint16_t pulse_us, int64_t timestamp_us)
{
    uint32_t id;
    portENTER_CRITICAL(&s_trace_lock);
    const uint32_t slot = s_write_index;
    id = s_next_id++;
    s_records[slot] = (servo_zero_trace_record_t){
        .id = id,
        .sequence = sequence,
        .pulse_us = pulse_us,
        .kind = kind,
        .esp_rx_us = kind == SERVO_ZERO_TRACE_KIND_COMMAND ? timestamp_us : 0,
        .ctrl_accept_us = kind == SERVO_ZERO_TRACE_KIND_TICK ? timestamp_us : 0,
        .i2c_begin_us = 0,
        .i2c_end_us = 0,
        .rx_interval_us = 0,
        .tick_interval_us = 0,
        .i2c_result = 0,
    };
    if (kind == SERVO_ZERO_TRACE_KIND_COMMAND && s_last_rx_us != 0) {
        s_records[slot].rx_interval_us = timestamp_us - s_last_rx_us;
    }
    if (kind == SERVO_ZERO_TRACE_KIND_TICK && s_last_tick_us != 0) {
        s_records[slot].tick_interval_us = timestamp_us - s_last_tick_us;
    }
    if (s_record_count < SERVO_ZERO_TRACE_CAPACITY) {
        s_record_count++;
    } else {
        s_dropped_count++;
    }
    s_write_index = (s_write_index + 1u) % SERVO_ZERO_TRACE_CAPACITY;
    if (kind == SERVO_ZERO_TRACE_KIND_COMMAND) {
        s_last_rx_us = timestamp_us;
    } else if (kind == SERVO_ZERO_TRACE_KIND_TICK) {
        s_last_tick_us = timestamp_us;
    }
    portEXIT_CRITICAL(&s_trace_lock);
    return id;
}

void servo_zero_trace_begin(servo_zero_trace_mode_t mode)
{
    portENTER_CRITICAL(&s_trace_lock);
    s_mode = mode;
    s_next_id = 0;
    s_write_index = 0;
    s_record_count = 0;
    s_dropped_count = 0;
    s_current_id = SERVO_ZERO_TRACE_INVALID_ID;
    s_last_rx_us = 0;
    s_last_tick_us = 0;
    s_i2c_count = 0;
    s_i2c_error_count = 0;
    s_i2c_interval_count = 0;
    s_i2c_duration_sum_us = 0;
    s_i2c_duration_max_us = 0;
    s_i2c_interval_sum_us = 0;
    s_i2c_interval_max_us = 0;
    s_last_i2c_begin_us = 0;
    portEXIT_CRITICAL(&s_trace_lock);
}

void servo_zero_trace_halt(void)
{
    portENTER_CRITICAL(&s_trace_lock);
    s_current_id = SERVO_ZERO_TRACE_INVALID_ID;
    portEXIT_CRITICAL(&s_trace_lock);
}

bool servo_zero_trace_is_active(void)
{
    bool active;
    portENTER_CRITICAL(&s_trace_lock);
    active = s_mode != SERVO_ZERO_TRACE_OFF;
    portEXIT_CRITICAL(&s_trace_lock);
    return active;
}

servo_zero_trace_mode_t servo_zero_trace_mode(void)
{
    servo_zero_trace_mode_t mode;
    portENTER_CRITICAL(&s_trace_lock);
    mode = s_mode;
    portEXIT_CRITICAL(&s_trace_lock);
    return mode;
}

uint32_t servo_zero_trace_command_rx(uint32_t sequence, uint16_t pulse_us)
{
    if (!servo_zero_trace_is_active()) {
        return SERVO_ZERO_TRACE_INVALID_ID;
    }
    return append_record(SERVO_ZERO_TRACE_KIND_COMMAND, sequence, pulse_us,
                         esp_timer_get_time());
}

void servo_zero_trace_control_accept(uint32_t record_id)
{
    if (record_id == SERVO_ZERO_TRACE_INVALID_ID) {
        return;
    }
    const int64_t timestamp_us = esp_timer_get_time();
    portENTER_CRITICAL(&s_trace_lock);
    servo_zero_trace_record_t *record = find_record_locked(record_id);
    if (record != NULL) {
        record->ctrl_accept_us = timestamp_us;
    }
    portEXIT_CRITICAL(&s_trace_lock);
}

uint32_t servo_zero_trace_control_tick(void)
{
    if (!servo_zero_trace_is_active()) {
        return SERVO_ZERO_TRACE_INVALID_ID;
    }
    return append_record(SERVO_ZERO_TRACE_KIND_TICK, 0, 0,
                         esp_timer_get_time());
}

void servo_zero_trace_set_current(uint32_t record_id)
{
    portENTER_CRITICAL(&s_trace_lock);
    s_current_id = record_id;
    portEXIT_CRITICAL(&s_trace_lock);
}

void servo_zero_trace_i2c_begin(void)
{
    const int64_t timestamp_us = esp_timer_get_time();
    portENTER_CRITICAL(&s_trace_lock);
    if (s_last_i2c_begin_us != 0) {
        const int64_t interval_us = timestamp_us - s_last_i2c_begin_us;
        s_i2c_interval_count++;
        s_i2c_interval_sum_us += interval_us;
        if (interval_us > s_i2c_interval_max_us) {
            s_i2c_interval_max_us = interval_us;
        }
    }
    s_last_i2c_begin_us = timestamp_us;
    s_i2c_count++;
    servo_zero_trace_record_t *record = find_record_locked(s_current_id);
    if (record != NULL) {
        record->i2c_begin_us = timestamp_us;
    }
    portEXIT_CRITICAL(&s_trace_lock);
}

void servo_zero_trace_i2c_end(int32_t result)
{
    const int64_t timestamp_us = esp_timer_get_time();
    portENTER_CRITICAL(&s_trace_lock);
    if (result != 0) {
        s_i2c_error_count++;
    }
    servo_zero_trace_record_t *record = find_record_locked(s_current_id);
    if (record != NULL) {
        record->i2c_end_us = timestamp_us;
        record->i2c_result = result;
        const int64_t duration_us = timestamp_us - record->i2c_begin_us;
        s_i2c_duration_sum_us += duration_us;
        if (duration_us > s_i2c_duration_max_us) {
            s_i2c_duration_max_us = duration_us;
        }
    }
    portEXIT_CRITICAL(&s_trace_lock);
}

static void print_record(const servo_zero_trace_record_t *record)
{
    if (record->kind == SERVO_ZERO_TRACE_KIND_COMMAND) {
        printf("ESP_RX seq=%lu timestamp_us=%lld pulse=%u rx_interval_us=%lld\n",
               (unsigned long)record->sequence,
               (long long)record->esp_rx_us, (unsigned)record->pulse_us,
               (long long)record->rx_interval_us);
        if (record->ctrl_accept_us != 0) {
            printf("CTRL_ACCEPT seq=%lu timestamp_us=%lld\n",
                   (unsigned long)record->sequence,
                   (long long)record->ctrl_accept_us);
        }
    } else {
        printf("CONTROL_TICK id=%lu timestamp_us=%lld tick_interval_us=%lld\n",
               (unsigned long)record->id,
               (long long)record->ctrl_accept_us,
               (long long)record->tick_interval_us);
    }
    if (record->i2c_begin_us != 0) {
        printf("I2C_BEGIN seq=%lu timestamp_us=%lld\n",
               (unsigned long)record->sequence,
               (long long)record->i2c_begin_us);
    }
    if (record->i2c_end_us != 0) {
        printf("I2C_END seq=%lu timestamp_us=%lld result=0x%lx duration_us=%lld\n",
               (unsigned long)record->sequence,
               (long long)record->i2c_end_us,
               (unsigned long)(uint32_t)record->i2c_result,
               (long long)(record->i2c_end_us - record->i2c_begin_us));
    }
}

void servo_zero_trace_dump(void)
{
    uint32_t count;
    uint32_t dropped;
    servo_zero_trace_mode_t mode;
    uint32_t i2c_count;
    uint32_t i2c_error_count;
    uint32_t i2c_interval_count;
    int64_t i2c_duration_sum_us;
    int64_t i2c_duration_max_us;
    int64_t i2c_interval_sum_us;
    int64_t i2c_interval_max_us;
    portENTER_CRITICAL(&s_trace_lock);
    count = s_record_count;
    dropped = s_dropped_count;
    mode = s_mode;
    i2c_count = s_i2c_count;
    i2c_error_count = s_i2c_error_count;
    i2c_interval_count = s_i2c_interval_count;
    i2c_duration_sum_us = s_i2c_duration_sum_us;
    i2c_duration_max_us = s_i2c_duration_max_us;
    i2c_interval_sum_us = s_i2c_interval_sum_us;
    i2c_interval_max_us = s_i2c_interval_max_us;
    portEXIT_CRITICAL(&s_trace_lock);

    printf("TRACE_DUMP_BEGIN mode=%u records=%lu dropped=%lu\n",
           (unsigned)mode, (unsigned long)count, (unsigned long)dropped);
    printf("TRACE_I2C count=%lu errors=%lu duration_mean_us=%lld duration_max_us=%lld interval_mean_us=%lld interval_max_us=%lld\n",
           (unsigned long)i2c_count, (unsigned long)i2c_error_count,
           i2c_count == 0 ? 0LL : (long long)(i2c_duration_sum_us / i2c_count),
           (long long)i2c_duration_max_us,
           i2c_interval_count == 0
               ? 0LL
               : (long long)(i2c_interval_sum_us / i2c_interval_count),
           (long long)i2c_interval_max_us);
    printf("TRACE_DUMP_END\n");
    fflush(stdout);
}

void servo_zero_trace_dump_page(uint32_t offset, uint32_t requested_count)
{
    uint32_t count;
    uint32_t dropped;
    servo_zero_trace_mode_t mode;
    portENTER_CRITICAL(&s_trace_lock);
    count = s_record_count;
    dropped = s_dropped_count;
    mode = s_mode;
    portEXIT_CRITICAL(&s_trace_lock);

    const uint32_t page_count =
        offset < count
            ? (requested_count > 32u ? 32u :
               (requested_count < count - offset ? requested_count :
                                                   count - offset))
            : 0u;
    printf("TRACE_PAGE_BEGIN mode=%u offset=%lu records=%lu dropped=%lu\n",
           (unsigned)mode, (unsigned long)offset, (unsigned long)page_count,
           (unsigned long)dropped);
    for (uint32_t index = 0; index < page_count; ++index) {
        servo_zero_trace_record_t record;
        portENTER_CRITICAL(&s_trace_lock);
        const uint32_t slot =
            (s_write_index + SERVO_ZERO_TRACE_CAPACITY - count + offset +
             index) %
            SERVO_ZERO_TRACE_CAPACITY;
        record = s_records[slot];
        portEXIT_CRITICAL(&s_trace_lock);
        print_record(&record);
        /* UART/USB output is much slower than the trace producer. Yield after
         * every record so the dump cannot starve IDLE and trip the watchdog. */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    printf("TRACE_PAGE_END\n");
    fflush(stdout);
}
