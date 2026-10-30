#ifndef NIULAI_SERVO_UART_PROTOCOL_H
#define NIULAI_SERVO_UART_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SERVO_UART_SOF 0xa5u
#define SERVO_UART_ACK_SOF 0x5au
#define SERVO_UART_VERSION 1u
#define SERVO_UART_CHANNEL_COUNT 8u
#define SERVO_UART_SET_TARGET 1u
#define SERVO_UART_RUN 2u
#define SERVO_UART_STOP 3u
#define SERVO_UART_ACK 0x80u
#define SERVO_UART_NACK 0x81u
#define SERVO_UART_STATUS_OK 0u
#define SERVO_UART_STATUS_BAD_CRC 1u
#define SERVO_UART_STATUS_BAD_VERSION 2u
#define SERVO_UART_STATUS_BAD_COMMAND 3u
#define SERVO_UART_STATUS_BAD_RANGE 4u
#define SERVO_UART_STATUS_BUSY 5u
#define SERVO_UART_STATUS_IO 6u
#define SERVO_UART_CONTROL_FRAME_BYTES 8u
#define SERVO_UART_SET_FRAME_BYTES 24u
#define SERVO_UART_ACK_FRAME_BYTES 8u
#define SERVO_UART_MAX_FRAME_BYTES SERVO_UART_SET_FRAME_BYTES

typedef struct {
    uint8_t bytes[SERVO_UART_MAX_FRAME_BYTES];
    uint8_t length;
    uint8_t expected_length;
} servo_uart_parser_t;

typedef struct {
    uint8_t command;
    uint16_t sequence;
    uint8_t channel_mask;
    uint16_t pulse_us[SERVO_UART_CHANNEL_COUNT];
    bool recognized;
    bool crc_valid;
} servo_uart_event_t;

uint16_t servo_uart_crc16(const uint8_t *data, size_t length);
size_t servo_uart_encode_set_target(
    uint16_t sequence, uint8_t channel_mask,
    const uint16_t pulse_us[SERVO_UART_CHANNEL_COUNT],
    uint8_t out[SERVO_UART_SET_FRAME_BYTES]);
size_t servo_uart_encode_control(uint8_t command, uint16_t sequence,
                                 uint8_t out[SERVO_UART_CONTROL_FRAME_BYTES]);
size_t servo_uart_encode_ack(uint16_t sequence, uint8_t status,
                             uint8_t out[SERVO_UART_ACK_FRAME_BYTES]);
void servo_uart_parser_init(servo_uart_parser_t *parser);
bool servo_uart_parser_active(const servo_uart_parser_t *parser);
bool servo_uart_parser_feed(servo_uart_parser_t *parser, uint8_t byte,
                            servo_uart_event_t *event);

#endif
