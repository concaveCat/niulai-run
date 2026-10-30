#include "sdkconfig.h"

#include "servo_zero_tcp_diag.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"

#define SERVO_ZERO_TCP_DIAG_QUEUE_LENGTH 32u
#define SERVO_ZERO_TCP_DIAG_MESSAGE_SIZE 256u
#define SERVO_ZERO_TCP_DIAG_POLL_MS 20u

typedef struct {
    char line[SERVO_ZERO_TCP_DIAG_MAX_LINE + 1u];
} diag_command_t;

typedef struct {
    char data[SERVO_ZERO_TCP_DIAG_MESSAGE_SIZE];
} diag_message_t;

static const char *TAG = "tcp_diag";
static QueueHandle_t s_command_queue;
static QueueHandle_t s_message_queue;
static vprintf_like_t s_previous_vprintf;
static bool s_server_started;

static int diag_vprintf(const char *format, va_list args)
{
    char line[SERVO_ZERO_TCP_DIAG_MESSAGE_SIZE];
    va_list copy;
    va_copy(copy, args);
    int length = vsnprintf(line, sizeof(line), format, copy);
    va_end(copy);

    if (length >= 0 && s_message_queue != NULL) {
        size_t used = (size_t)length;
        if (used >= sizeof(line)) {
            used = sizeof(line) - 1u;
        }
        if (used == 0u || line[used - 1u] != '\n') {
            if (used + 1u < sizeof(line)) {
                line[used++] = '\n';
                line[used] = '\0';
            }
        }
        diag_message_t message = {0};
        memcpy(message.data, line, used + 1u);
        (void)xQueueSend(s_message_queue, &message, 0);
    }

    if (s_previous_vprintf != NULL) {
        return s_previous_vprintf(format, args);
    }
    return vprintf(format, args);
}

static void close_client(int *client_fd)
{
    if (*client_fd >= 0) {
        shutdown(*client_fd, SHUT_RDWR);
        close(*client_fd);
        *client_fd = -1;
    }
}

static bool send_all(int client_fd, const char *data, size_t length)
{
    size_t sent = 0u;
    while (sent < length) {
        const int result = send(client_fd, data + sent, length - sent, 0);
        if (result <= 0) {
            return false;
        }
        sent += (size_t)result;
    }
    return true;
}

static void drain_messages(int *client_fd)
{
    diag_message_t message;
    while (xQueueReceive(s_message_queue, &message, 0) == pdTRUE) {
        if (*client_fd < 0) {
            continue;
        }
        if (!send_all(*client_fd, message.data, strnlen(message.data,
                                                        sizeof(message.data)))) {
            close_client(client_fd);
            return;
        }
    }
}

static void accept_command(char *line, size_t length)
{
    while (length > 0u && (line[length - 1u] == '\r' ||
                           line[length - 1u] == '\n')) {
        line[--length] = '\0';
    }
    if (length == 0u) {
        return;
    }

    diag_command_t command = {0};
    memcpy(command.line, line, length + 1u);
    if (xQueueSend(s_command_queue, &command, 0) != pdTRUE) {
        ESP_LOGW(TAG, "TCP_DIAG command queue full; dropped command");
    }
}

static void receive_commands(int *client_fd, char *line, size_t *line_length,
                             bool *discard_until_newline)
{
    char input[64];
    const int received = recv(*client_fd, input, sizeof(input), MSG_DONTWAIT);
    if (received == 0) {
        close_client(client_fd);
        return;
    }
    if (received < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            close_client(client_fd);
        }
        return;
    }

    for (int index = 0; index < received; ++index) {
        const char character = input[index];
        if (character == '\r' || character == '\n') {
            if (!*discard_until_newline) {
                line[*line_length] = '\0';
                accept_command(line, *line_length);
            }
            *line_length = 0u;
            *discard_until_newline = false;
        } else if (!*discard_until_newline) {
            if (*line_length < SERVO_ZERO_TCP_DIAG_MAX_LINE) {
                line[(*line_length)++] = character;
            } else {
                *line_length = 0u;
                *discard_until_newline = true;
                static const char too_long[] =
                    "TCP_DIAG_ERR command too long\n";
                (void)send_all(*client_fd, too_long, sizeof(too_long) - 1u);
            }
        }
    }
}

static void tcp_diag_task(void *arg)
{
    (void)arg;
    int listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_fd < 0) {
        ESP_LOGE(TAG, "TCP_DIAG socket create failed errno=%d", errno);
        vTaskDelete(NULL);
        return;
    }

    int reuse = 1;
    (void)setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(CONFIG_SERVO_ZERO_TCP_DIAG_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        listen(listen_fd, 1) < 0) {
        ESP_LOGE(TAG, "TCP_DIAG bind/listen failed errno=%d", errno);
        close(listen_fd);
        vTaskDelete(NULL);
        return;
    }

    (void)fcntl(listen_fd, F_SETFL, O_NONBLOCK);
    int client_fd = -1;
    char line[SERVO_ZERO_TCP_DIAG_MAX_LINE + 1u] = {0};
    size_t line_length = 0u;
    bool discard_until_newline = false;
    ESP_LOGW(TAG, "TCP_DIAG_LISTEN port=%d", CONFIG_SERVO_ZERO_TCP_DIAG_PORT);

    for (;;) {
        if (client_fd < 0) {
            struct sockaddr_in client_address;
            socklen_t client_length = sizeof(client_address);
            client_fd = accept(listen_fd, (struct sockaddr *)&client_address,
                               &client_length);
            if (client_fd >= 0) {
                struct timeval send_timeout = {
                    .tv_sec = 0,
                    .tv_usec = 200000,
                };
                (void)setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO,
                                 &send_timeout, sizeof(send_timeout));
                line_length = 0u;
                discard_until_newline = false;
                static const char banner[] =
                    "TCP_DIAG_CONNECTED; send ASCII commands ending LF\n";
                if (!send_all(client_fd, banner, sizeof(banner) - 1u)) {
                    close_client(&client_fd);
                } else {
                    ESP_LOGW(TAG, "TCP_DIAG_CLIENT_CONNECTED");
                }
            }
            drain_messages(&client_fd);
            vTaskDelay(pdMS_TO_TICKS(SERVO_ZERO_TCP_DIAG_POLL_MS));
            continue;
        }

        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(client_fd, &read_set);
        struct timeval wait_time = {
            .tv_sec = 0,
            .tv_usec = SERVO_ZERO_TCP_DIAG_POLL_MS * 1000,
        };
        const int ready = select(client_fd + 1, &read_set, NULL, NULL,
                                 &wait_time);
        if (ready < 0 && errno != EINTR) {
            close_client(&client_fd);
        } else if (ready > 0 && FD_ISSET(client_fd, &read_set)) {
            receive_commands(&client_fd, line, &line_length,
                             &discard_until_newline);
        }
        drain_messages(&client_fd);
    }
}

static void start_softap(void)
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "TCP_DIAG esp_netif_init failed: %s", esp_err_to_name(err));
        return;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "TCP_DIAG event loop failed: %s", esp_err_to_name(err));
        return;
    }
    if (esp_netif_create_default_wifi_ap() == NULL) {
        ESP_LOGE(TAG, "TCP_DIAG AP netif create failed");
        return;
    }

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_config);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "TCP_DIAG esp_wifi_init failed: %s", esp_err_to_name(err));
        return;
    }

    wifi_config_t ap_config = {0};
    const char *ssid = CONFIG_SERVO_ZERO_TCP_DIAG_WIFI_SSID;
    const char *password = CONFIG_SERVO_ZERO_TCP_DIAG_WIFI_PASSWORD;
    strncpy((char *)ap_config.ap.ssid, ssid, sizeof(ap_config.ap.ssid));
    strncpy((char *)ap_config.ap.password, password,
            sizeof(ap_config.ap.password));
    ap_config.ap.ssid_len = strlen(ssid);
    ap_config.ap.channel = 1;
    ap_config.ap.max_connection = 1;
    ap_config.ap.authmode = strlen(password) >= 8u ? WIFI_AUTH_WPA2_PSK
                                                   : WIFI_AUTH_OPEN;

    if (esp_wifi_set_mode(WIFI_MODE_AP) != ESP_OK ||
        esp_wifi_set_config(WIFI_IF_AP, &ap_config) != ESP_OK ||
        esp_wifi_start() != ESP_OK) {
        ESP_LOGE(TAG, "TCP_DIAG Wi-Fi AP start failed");
        return;
    }

    ESP_LOGW(TAG, "TCP_DIAG_AP SSID=%s IP=192.168.4.1 PORT=%d", ssid,
             CONFIG_SERVO_ZERO_TCP_DIAG_PORT);
}

void servo_zero_tcp_diag_init(void)
{
#if !CONFIG_SERVO_ZERO_TCP_DIAG_ENABLED
    return;
#else
    if (s_server_started) {
        return;
    }
    s_command_queue = xQueueCreate(SERVO_ZERO_TCP_DIAG_QUEUE_LENGTH,
                                   sizeof(diag_command_t));
    s_message_queue = xQueueCreate(SERVO_ZERO_TCP_DIAG_QUEUE_LENGTH,
                                   sizeof(diag_message_t));
    if (s_command_queue == NULL || s_message_queue == NULL) {
        ESP_LOGE(TAG, "TCP_DIAG queue allocation failed");
        return;
    }

    s_previous_vprintf = esp_log_set_vprintf(diag_vprintf);
    start_softap();
    if (xTaskCreate(tcp_diag_task, "tcp_diag", 4096, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "TCP_DIAG task creation failed");
        return;
    }
    s_server_started = true;
#endif
}

bool servo_zero_tcp_diag_take_command(char *out, size_t out_size)
{
    if (out == NULL || out_size == 0u || s_command_queue == NULL) {
        return false;
    }
    diag_command_t command;
    if (xQueueReceive(s_command_queue, &command, 0) != pdTRUE) {
        return false;
    }
    const size_t length = strnlen(command.line, sizeof(command.line));
    if (length + 1u > out_size) {
        return false;
    }
    memcpy(out, command.line, length + 1u);
    return true;
}

void servo_zero_tcp_diag_reply(const char *message)
{
    if (message == NULL || s_message_queue == NULL) {
        return;
    }
    diag_message_t queued = {0};
    size_t length = strnlen(message, sizeof(queued.data) - 2u);
    memcpy(queued.data, message, length);
    if (length == 0u || queued.data[length - 1u] != '\n') {
        queued.data[length++] = '\n';
    }
    queued.data[length] = '\0';
    (void)xQueueSend(s_message_queue, &queued, 0);
}

void servo_zero_tcp_diag_replyf(const char *format, ...)
{
    char message[SERVO_ZERO_TCP_DIAG_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    servo_zero_tcp_diag_reply(message);
}
