#ifndef PICO_LCD_PLATFORM_SSH_H
#define PICO_LCD_PLATFORM_SSH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SSH_HOST_CAPACITY 65
#define SSH_USERNAME_CAPACITY 33
#define SSH_PASSWORD_CAPACITY 65
#define SSH_FINGERPRINT_CAPACITY 96
#define SSH_MESSAGE_CAPACITY 64
#define SSH_TERMINAL_CAPACITY 768
#define SSH_LINE_CAPACITY 65

typedef enum SshStatus
{
    SSH_STATUS_UNAVAILABLE,
    SSH_STATUS_IDLE,
    SSH_STATUS_CONNECTING,
    SSH_STATUS_VERIFY_HOST,
    SSH_STATUS_AUTHENTICATING,
    SSH_STATUS_CONNECTED,
    SSH_STATUS_BAD_CREDENTIALS,
    SSH_STATUS_DISCONNECTED,
    SSH_STATUS_ERROR,
} SshStatus;

typedef struct SshSnapshot
{
    SshStatus status;
    uint32_t revision;
    char fingerprint[SSH_FINGERPRINT_CAPACITY];
    char message[SSH_MESSAGE_CAPACITY];
    char terminal[SSH_TERMINAL_CAPACITY];
} SshSnapshot;

typedef struct SshServiceApi
{
    void (*get_snapshot)(SshSnapshot *snapshot);
    bool (*request_connect)(
        const char *host,
        uint16_t port,
        const char *username,
        const char *password);
    bool (*request_host_verification)(bool accepted);
    bool (*request_send_line)(const char *line);
    bool (*request_disconnect)(void);
} SshServiceApi;

const char *ssh_status_name(SshStatus status);

#endif
