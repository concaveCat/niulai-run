#include <assert.h>
#include <stdint.h>

#include "servo_uart_protocol.h"

int main(void)
{
    const uint16_t pulses[8] = {1746u, 1469u, 1427u, 1415u,
                                1619u, 1582u, 1449u, 1600u};
    uint8_t frame[SERVO_UART_SET_FRAME_BYTES];
    servo_uart_parser_t parser;
    servo_uart_event_t event;

    assert(servo_uart_crc16((const uint8_t *)"123456789", 9u) == 0x29b1u);
    assert(servo_uart_encode_set_target(0x1234u, 0xffu, pulses, frame) ==
           SERVO_UART_SET_FRAME_BYTES);
    servo_uart_parser_init(&parser);
    for (size_t i = 0; i < sizeof(frame) - 1u; ++i) {
        assert(!servo_uart_parser_feed(&parser, frame[i], &event));
    }
    assert(servo_uart_parser_feed(&parser, frame[sizeof(frame) - 1u], &event));
    assert(event.recognized && event.crc_valid);
    assert(event.sequence == 0x1234u && event.channel_mask == 0xffu);
    assert(event.pulse_us[0] == 1746u && event.pulse_us[7] == 1600u);

    frame[10] ^= 1u;
    servo_uart_parser_init(&parser);
    for (size_t i = 0; i < sizeof(frame) - 1u; ++i) {
        (void)servo_uart_parser_feed(&parser, frame[i], &event);
    }
    assert(servo_uart_parser_feed(&parser, frame[sizeof(frame) - 1u], &event));
    assert(!event.crc_valid);

    /* Control frames must keep the reserved byte clear. */
    uint8_t control[SERVO_UART_CONTROL_FRAME_BYTES];
    servo_uart_encode_control(SERVO_UART_RUN, 9u, control);
    control[5] = 1u;
    control[SERVO_UART_CONTROL_FRAME_BYTES - 2u] =
        (uint8_t)(servo_uart_crc16(control,
                                   SERVO_UART_CONTROL_FRAME_BYTES - 2u) &
                  0xffu);
    control[SERVO_UART_CONTROL_FRAME_BYTES - 1u] =
        (uint8_t)(servo_uart_crc16(control,
                                   SERVO_UART_CONTROL_FRAME_BYTES - 2u) >>
                  8);
    servo_uart_parser_init(&parser);
    for (size_t i = 0; i < sizeof(control) - 1u; ++i) {
        (void)servo_uart_parser_feed(&parser, control[i], &event);
    }
    assert(servo_uart_parser_feed(&parser, control[sizeof(control) - 1u],
                                  &event));
    assert(!event.recognized && event.crc_valid);
    return 0;
}
