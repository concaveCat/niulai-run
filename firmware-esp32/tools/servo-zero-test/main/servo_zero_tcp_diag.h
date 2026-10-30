#ifndef NIULAI_SERVO_ZERO_TCP_DIAG_H
#define NIULAI_SERVO_ZERO_TCP_DIAG_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SERVO_ZERO_TCP_DIAG_MAX_LINE 127u

/* Starts the local SoftAP and TCP log/command bridge. Failure is non-fatal:
 * the UART CLI remains available. */
void servo_zero_tcp_diag_init(void);

/* The main task owns command execution because the existing command parser
 * may perform I2C and motion work. This function is non-blocking. */
bool servo_zero_tcp_diag_take_command(char *out, size_t out_size);

/* Queue a newline-terminated diagnostic message for the connected TCP peer.
 * Messages are best-effort and are dropped when the peer/queue is unavailable.
 */
void servo_zero_tcp_diag_reply(const char *message);
void servo_zero_tcp_diag_replyf(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
