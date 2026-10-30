#include "servo_uart_protocol.h"

#include <string.h>

static void put_u16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xffu);
    dst[1] = (uint8_t)(value >> 8);
}

static uint16_t get_u16(const uint8_t *src)
{
    return (uint16_t)src[0] | (uint16_t)((uint16_t)src[1] << 8);
}

uint16_t servo_uart_crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xffffu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8u; ++bit) {
            crc = (crc & 0x8000u) != 0u
                      ? (uint16_t)((crc << 1) ^ 0x1021u)
                      : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static void finish_frame(uint8_t *frame, size_t length)
{
    put_u16(&frame[length - 2u], servo_uart_crc16(frame, length - 2u));
}

size_t servo_uart_encode_set_target(
    uint16_t sequence, uint8_t channel_mask,
    const uint16_t pulse_us[SERVO_UART_CHANNEL_COUNT],
    uint8_t out[SERVO_UART_SET_FRAME_BYTES])
{
    out[0] = SERVO_UART_SOF;
    out[1] = SERVO_UART_VERSION;
    out[2] = SERVO_UART_SET_TARGET;
    put_u16(&out[3], sequence);
    out[5] = channel_mask;
    for (uint8_t channel = 0; channel < SERVO_UART_CHANNEL_COUNT; ++channel) {
        put_u16(&out[6u + 2u * channel], pulse_us[channel]);
    }
    finish_frame(out, SERVO_UART_SET_FRAME_BYTES);
    return SERVO_UART_SET_FRAME_BYTES;
}

size_t servo_uart_encode_control(uint8_t command, uint16_t sequence,
                                 uint8_t out[SERVO_UART_CONTROL_FRAME_BYTES])
{
    out[0] = SERVO_UART_SOF;
    out[1] = SERVO_UART_VERSION;
    out[2] = command;
    put_u16(&out[3], sequence);
    out[5] = 0u;
    finish_frame(out, SERVO_UART_CONTROL_FRAME_BYTES);
    return SERVO_UART_CONTROL_FRAME_BYTES;
}

size_t servo_uart_encode_ack(uint16_t sequence, uint8_t status,
                             uint8_t out[SERVO_UART_ACK_FRAME_BYTES])
{
    out[0] = SERVO_UART_ACK_SOF;
    out[1] = SERVO_UART_VERSION;
    out[2] = status == SERVO_UART_STATUS_OK ? SERVO_UART_ACK : SERVO_UART_NACK;
    put_u16(&out[3], sequence);
    out[5] = status;
    finish_frame(out, SERVO_UART_ACK_FRAME_BYTES);
    return SERVO_UART_ACK_FRAME_BYTES;
}

void servo_uart_parser_init(servo_uart_parser_t *parser)
{
    memset(parser, 0, sizeof(*parser));
}

bool servo_uart_parser_active(const servo_uart_parser_t *parser)
{
    return parser != NULL && parser->length != 0u;
}

bool servo_uart_parser_feed(servo_uart_parser_t *parser, uint8_t byte,
                            servo_uart_event_t *event)
{
    if (parser == NULL || event == NULL) {
        return false;
    }
    if (parser->length == 0u) {
        if (byte != SERVO_UART_SOF) {
            return false;
        }
        parser->bytes[0] = byte;
        parser->length = 1u;
        parser->expected_length = 0u;
        return false;
    }
    if (parser->length >= SERVO_UART_MAX_FRAME_BYTES) {
        servo_uart_parser_init(parser);
        return false;
    }
    parser->bytes[parser->length++] = byte;
    if (parser->length == 3u) {
        parser->expected_length = parser->bytes[2] == SERVO_UART_SET_TARGET
                                      ? SERVO_UART_SET_FRAME_BYTES
                                      : SERVO_UART_CONTROL_FRAME_BYTES;
    }
    if (parser->expected_length == 0u ||
        parser->length < parser->expected_length) {
        return false;
    }

    memset(event, 0, sizeof(*event));
    event->command = parser->bytes[2];
    event->sequence = get_u16(&parser->bytes[3]);
    const bool control_reserved_ok =
        event->command == SERVO_UART_SET_TARGET || parser->bytes[5] == 0u;
    event->recognized = parser->bytes[1] == SERVO_UART_VERSION &&
                        control_reserved_ok &&
                        (event->command == SERVO_UART_SET_TARGET ||
                         event->command == SERVO_UART_RUN ||
                         event->command == SERVO_UART_STOP);
    event->crc_valid = get_u16(&parser->bytes[parser->expected_length - 2u]) ==
                       servo_uart_crc16(parser->bytes,
                                        parser->expected_length - 2u);
    if (event->command == SERVO_UART_SET_TARGET &&
        parser->expected_length == SERVO_UART_SET_FRAME_BYTES) {
        event->channel_mask = parser->bytes[5];
        for (uint8_t channel = 0; channel < SERVO_UART_CHANNEL_COUNT;
             ++channel) {
            event->pulse_us[channel] = get_u16(&parser->bytes[6u + 2u * channel]);
        }
    }
    servo_uart_parser_init(parser);
    return true;
}
