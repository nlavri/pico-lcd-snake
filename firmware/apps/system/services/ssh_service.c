#include "ssh_service.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "common/logger.h"

#define LOG_COMPONENT "ssh"
#define SSH_COMMAND_QUEUE_LENGTH 3
#define SSH_TASK_STACK_DEPTH 2048
#define SSH_TASK_PRIORITY 3

typedef enum SshCommandType
{
    SSH_COMMAND_CONNECT,
    SSH_COMMAND_VERIFY_HOST,
    SSH_COMMAND_SEND_LINE,
    SSH_COMMAND_DISCONNECT,
} SshCommandType;

typedef struct SshCommand
{
    SshCommandType type;
    char host[SSH_HOST_CAPACITY];
    char username[SSH_USERNAME_CAPACITY];
    char password[SSH_PASSWORD_CAPACITY];
    char line[SSH_LINE_CAPACITY];
    uint16_t port;
    bool accepted;
} SshCommand;

static SshSnapshot current_snapshot = {
    .status = SSH_STATUS_UNAVAILABLE,
};
static StaticQueue_t command_queue_storage;
static uint8_t command_queue_bytes[
    SSH_COMMAND_QUEUE_LENGTH * sizeof(SshCommand)];
static QueueHandle_t command_queue;
#if APP_SSH_PICO_W
static bool terminal_escape;
#endif

static void secure_clear(void *memory, size_t size)
{
    volatile unsigned char *bytes = memory;
    while (size-- > 0)
    {
        *bytes++ = 0;
    }
}

static void get_snapshot(SshSnapshot *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }
    taskENTER_CRITICAL();
    *snapshot = current_snapshot;
    taskEXIT_CRITICAL();
}

static bool enqueue(const SshCommand *command)
{
    return command_queue != NULL &&
        xQueueSend(command_queue, command, 0) == pdTRUE;
}

static bool request_connect(
    const char *host,
    uint16_t port,
    const char *username,
    const char *password)
{
    if (host == NULL || host[0] == '\0' || username == NULL ||
        username[0] == '\0' || password == NULL)
    {
        return false;
    }

    SshCommand command = {
        .type = SSH_COMMAND_CONNECT,
        .port = port == 0 ? 22 : port,
    };
    snprintf(command.host, sizeof(command.host), "%s", host);
    snprintf(command.username, sizeof(command.username), "%s", username);
    snprintf(command.password, sizeof(command.password), "%s", password);
    const bool queued = enqueue(&command);
    secure_clear(command.password, sizeof(command.password));
    return queued;
}

static bool request_host_verification(bool accepted)
{
    const SshCommand command = {
        .type = SSH_COMMAND_VERIFY_HOST,
        .accepted = accepted,
    };
    return enqueue(&command);
}

static bool request_send_line(const char *line)
{
    if (line == NULL)
    {
        return false;
    }
    SshCommand command = {.type = SSH_COMMAND_SEND_LINE};
    snprintf(command.line, sizeof(command.line), "%s", line);
    return enqueue(&command);
}

static bool request_disconnect(void)
{
    const SshCommand command = {.type = SSH_COMMAND_DISCONNECT};
    return enqueue(&command);
}

static const SshServiceApi service_api = {
    .get_snapshot = get_snapshot,
    .request_connect = request_connect,
    .request_host_verification = request_host_verification,
    .request_send_line = request_send_line,
    .request_disconnect = request_disconnect,
};

#if APP_SSH_PICO_W

#include <stdlib.h>

#include "libssh2.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "mbedtls/platform.h"

/* lwIP provides its own global errno while newlib exposes a task-local errno
 * macro. Socket callbacks must read lwIP's value directly. */
extern int pico_lwip_errno __asm__("errno");

typedef union AllocationHeader
{
    struct
    {
        uint32_t magic;
        size_t size;
    } metadata;
    uint64_t alignment;
} AllocationHeader;

#define SSH_ALLOCATION_MAGIC 0x53534841u

static bool heap_can_allocate(size_t size)
{
    HeapStats_t stats;
    vPortGetHeapStats(&stats);
    return size <= stats.xSizeOfLargestFreeBlockInBytes &&
        stats.xSizeOfLargestFreeBlockInBytes - size >= 16;
}

static void *allocate_with_header(size_t count, bool clear)
{
    if (count > SIZE_MAX - sizeof(AllocationHeader))
    {
        return NULL;
    }
    const size_t total = sizeof(AllocationHeader) + count;
    if (!heap_can_allocate(total))
    {
        LOG_ERROR(
            LOG_COMPONENT,
            "libssh2 allocation rejected bytes=%u free_heap=%u",
            (unsigned int)total,
            (unsigned int)xPortGetFreeHeapSize());
        return NULL;
    }
    AllocationHeader *header = pvPortMalloc(total);
    if (header == NULL)
    {
        return NULL;
    }
    header->metadata.magic = SSH_ALLOCATION_MAGIC;
    header->metadata.size = count;
    void *memory = header + 1;
    if (clear)
    {
        memset(memory, 0, count);
    }
    return memory;
}

static void free_with_header(void *pointer, const char *owner)
{
    if (pointer == NULL)
    {
        return;
    }
    AllocationHeader *header = ((AllocationHeader *)pointer) - 1;
    if (header->metadata.magic != SSH_ALLOCATION_MAGIC)
    {
        LOG_ERROR(
            LOG_COMPONENT,
            "%s invalid or duplicate free pointer=%p magic=%08x",
            owner,
            pointer,
            (unsigned int)header->metadata.magic);
        return;
    }
    header->metadata.magic = 0;
    vPortFree(header);
}

static void *rtos_alloc(size_t count, void **abstract)
{
    (void)abstract;
    return allocate_with_header(count, false);
}

static void rtos_free(void *pointer, void **abstract)
{
    (void)abstract;
    free_with_header(pointer, "libssh2");
}

static void *rtos_realloc(void *pointer, size_t count, void **abstract)
{
    if (pointer == NULL)
    {
        return rtos_alloc(count, abstract);
    }
    if (count == 0)
    {
        rtos_free(pointer, abstract);
        return NULL;
    }

    AllocationHeader *old_header = ((AllocationHeader *)pointer) - 1;
    if (old_header->metadata.magic != SSH_ALLOCATION_MAGIC)
    {
        LOG_ERROR(LOG_COMPONENT, "libssh2 realloc received invalid pointer");
        return NULL;
    }
    void *replacement = rtos_alloc(count, abstract);
    if (replacement == NULL)
    {
        return NULL;
    }
    memcpy(
        replacement,
        pointer,
        old_header->metadata.size < count ?
            old_header->metadata.size : count);
    rtos_free(pointer, abstract);
    return replacement;
}

static void *rtos_calloc(size_t count, size_t size)
{
    if (size != 0 && count > SIZE_MAX / size)
    {
        return NULL;
    }
    return allocate_with_header(count * size, true);
}

static void rtos_plain_free(void *pointer)
{
    free_with_header(pointer, "mbedtls");
}

static ssize_t translate_socket_result(ssize_t result)
{
    if (result >= 0)
    {
        return result;
    }
    const int error = pico_lwip_errno;
    if (error == EAGAIN || error == EWOULDBLOCK ||
        error == EINTR || error == ENOENT)
    {
        return -EAGAIN;
    }
    return error == 0 ? -EIO : -error;
}

static LIBSSH2_RECV_FUNC(pico_ssh_recv)
{
    (void)abstract;
    return translate_socket_result(
        lwip_recv(socket, buffer, length, flags));
}

static LIBSSH2_SEND_FUNC(pico_ssh_send)
{
    (void)abstract;
    return translate_socket_result(
        lwip_send(socket, buffer, length, flags));
}

static void set_status(SshStatus status, const char *message)
{
    taskENTER_CRITICAL();
    current_snapshot.status = status;
    snprintf(
        current_snapshot.message,
        sizeof(current_snapshot.message),
        "%s",
        message == NULL ? "" : message);
    current_snapshot.revision++;
    taskEXIT_CRITICAL();
    LOG_INFO(
        LOG_COMPONENT,
        "status=%s message=%s",
        ssh_status_name(status),
        message == NULL ? "" : message);
}

static void clear_terminal(void)
{
    taskENTER_CRITICAL();
    current_snapshot.terminal[0] = '\0';
    current_snapshot.fingerprint[0] = '\0';
    current_snapshot.revision++;
    taskEXIT_CRITICAL();
    terminal_escape = false;
}

static void append_terminal(const char *data, size_t length)
{
    char cleaned[128];
    size_t output = 0;
    for (size_t i = 0; i < length && output + 1 < sizeof(cleaned); i++)
    {
        const unsigned char value = (unsigned char)data[i];
        if (terminal_escape)
        {
            if ((value >= 'A' && value <= 'Z') ||
                (value >= 'a' && value <= 'z'))
            {
                terminal_escape = false;
            }
            continue;
        }
        if (value == 0x1b)
        {
            terminal_escape = true;
        }
        else if (value == '\r')
        {
            continue;
        }
        else if (value == '\n' || (value >= 32 && value <= 126))
        {
            cleaned[output++] = (char)value;
        }
        else if (value == '\b' && output > 0)
        {
            output--;
        }
    }
    cleaned[output] = '\0';
    if (output == 0)
    {
        return;
    }

    taskENTER_CRITICAL();
    size_t used = strlen(current_snapshot.terminal);
    if (used + output >= sizeof(current_snapshot.terminal))
    {
        const size_t remove = used + output -
            sizeof(current_snapshot.terminal) + 1;
        memmove(
            current_snapshot.terminal,
            current_snapshot.terminal + remove,
            used - remove + 1);
        used -= remove;
    }
    memcpy(current_snapshot.terminal + used, cleaned, output + 1);
    current_snapshot.revision++;
    taskEXIT_CRITICAL();
}

static int open_socket(const char *host, uint16_t port)
{
    LOG_INFO(
        LOG_COMPONENT,
        "tcp resolve host=%s port=%u",
        host,
        (unsigned int)port);
    char service[6];
    snprintf(service, sizeof(service), "%u", (unsigned int)port);
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *addresses = NULL;
    const int resolve_result = getaddrinfo(
        host, service, &hints, &addresses);
    if (resolve_result != 0 ||
        addresses == NULL)
    {
        LOG_ERROR(
            LOG_COMPONENT,
            "tcp resolve failed code=%d",
            resolve_result);
        return -1;
    }

    LOG_INFO(LOG_COMPONENT, "tcp resolved; creating socket");

    const int socket_fd = socket(
        addresses->ai_family,
        addresses->ai_socktype,
        addresses->ai_protocol);
    if (socket_fd >= 0)
    {
        struct timeval timeout = {.tv_sec = 10, .tv_usec = 0};
        setsockopt(
            socket_fd, SOL_SOCKET, SO_RCVTIMEO,
            &timeout, sizeof(timeout));
        setsockopt(
            socket_fd, SOL_SOCKET, SO_SNDTIMEO,
            &timeout, sizeof(timeout));
        LOG_INFO(LOG_COMPONENT, "tcp connecting");
        if (connect(socket_fd, addresses->ai_addr, addresses->ai_addrlen) != 0)
        {
            LOG_ERROR(
                LOG_COMPONENT,
                "tcp connect failed errno=%d",
                errno);
            close(socket_fd);
            freeaddrinfo(addresses);
            return -1;
        }
        LOG_INFO(LOG_COMPONENT, "tcp connected fd=%d", socket_fd);
    }
    else
    {
        LOG_ERROR(
            LOG_COMPONENT,
            "socket creation failed errno=%d",
            errno);
    }
    freeaddrinfo(addresses);
    return socket_fd;
}

static void set_fingerprint(LIBSSH2_SESSION *session)
{
    const unsigned char *hash = (const unsigned char *)
        libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA256);
    if (hash == NULL)
    {
        return;
    }

    char fingerprint[SSH_FINGERPRINT_CAPACITY] = "SHA256 ";
    size_t offset = strlen(fingerprint);
    for (size_t i = 0; i < 32 && offset + 2 < sizeof(fingerprint); i++)
    {
        offset += (size_t)snprintf(
            fingerprint + offset,
            sizeof(fingerprint) - offset,
            "%02X",
            hash[i]);
    }

    taskENTER_CRITICAL();
    snprintf(
        current_snapshot.fingerprint,
        sizeof(current_snapshot.fingerprint),
        "%s",
        fingerprint);
    current_snapshot.revision++;
    taskEXIT_CRITICAL();
    LOG_INFO(LOG_COMPONENT, "host fingerprint=%s", fingerprint);
}

static bool wait_for_host_decision(void)
{
    SshCommand command;
    while (xQueueReceive(command_queue, &command, portMAX_DELAY) == pdTRUE)
    {
        if (command.type == SSH_COMMAND_VERIFY_HOST)
        {
            LOG_INFO(
                LOG_COMPONENT,
                "host verification decision=%s",
                command.accepted ? "accepted" : "rejected");
            return command.accepted;
        }
        if (command.type == SSH_COMMAND_DISCONNECT)
        {
            return false;
        }
    }
    return false;
}

static void close_session(
    int socket_fd,
    LIBSSH2_SESSION *session,
    LIBSSH2_CHANNEL *channel)
{
    (void)channel;

    /* The interactive terminal switches libssh2 and its lwIP socket to
     * non-blocking mode.  In that mode the graceful channel/session close
     * functions may return EAGAIN without freeing anything.  We cannot leave
     * those allocations behind: a second key exchange needs nearly all of
     * the Pico W's remaining heap.
     *
     * Give the peer one chance to receive SSH_MSG_DISCONNECT, then close the
     * transport.  With the socket gone, session_free can no longer stall on
     * EAGAIN while closing its owned channels and reliably releases every
     * local libssh2/mbedTLS allocation. */
    if (session != NULL)
    {
        libssh2_session_disconnect(session, "Pico terminal closed");
    }
    if (socket_fd >= 0)
    {
        close(socket_fd);
    }
    if (session != NULL)
    {
        const int free_result = libssh2_session_free(session);
        if (free_result != 0)
        {
            LOG_ERROR(
                LOG_COMPONENT,
                "session cleanup failed code=%d",
                free_result);
        }
    }

    LOG_INFO(LOG_COMPONENT, "connection cleanup complete");
}

static bool write_channel(
    LIBSSH2_SESSION *session,
    LIBSSH2_CHANNEL *channel,
    const char *data,
    size_t length)
{
    size_t offset = 0;
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(2000);
    while (offset < length)
    {
        const ssize_t written = libssh2_channel_write(
            channel, data + offset, length - offset);
        if (written > 0)
        {
            offset += (size_t)written;
            continue;
        }
        if (written != LIBSSH2_ERROR_EAGAIN ||
            (int32_t)(xTaskGetTickCount() - deadline) >= 0)
        {
            char *error_message = NULL;
            int error_length = 0;
            const int last_error = libssh2_session_last_error(
                session,
                &error_message,
                &error_length,
                0);
            LOG_ERROR(
                LOG_COMPONENT,
                "terminal write failed code=%d last_error=%d errno=%d message=%.*s",
                (int)written,
                last_error,
                errno,
                error_length,
                error_message == NULL ? "" : error_message);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return true;
}

static void run_connection(SshCommand *connect_command)
{
    int socket_fd = -1;
    LIBSSH2_SESSION *session = NULL;
    LIBSSH2_CHANNEL *channel = NULL;

    clear_terminal();
    LOG_INFO(
        LOG_COMPONENT,
        "connection requested host=%s port=%u",
        connect_command->host,
        (unsigned int)connect_command->port);
    set_status(SSH_STATUS_CONNECTING, "Opening TCP connection");
    socket_fd = open_socket(connect_command->host, connect_command->port);
    if (socket_fd < 0)
    {
        set_status(SSH_STATUS_ERROR, "Host unavailable");
        goto cleanup;
    }

    session = libssh2_session_init_ex(
        rtos_alloc, rtos_free, rtos_realloc, NULL);
    if (session == NULL)
    {
        set_status(SSH_STATUS_ERROR, "Out of memory");
        goto cleanup;
    }
    libssh2_session_callback_set2(
        session,
        LIBSSH2_CALLBACK_RECV,
        (libssh2_cb_generic *)pico_ssh_recv);
    libssh2_session_callback_set2(
        session,
        LIBSSH2_CALLBACK_SEND,
        (libssh2_cb_generic *)pico_ssh_send);
    LOG_INFO(LOG_COMPONENT, "starting SSH handshake");
    libssh2_session_set_timeout(session, 10000);
    const int handshake_result =
        libssh2_session_handshake(session, socket_fd);
    if (handshake_result != 0)
    {
        char *error_message = NULL;
        int error_length = 0;
        const int last_error = libssh2_session_last_error(
            session, &error_message, &error_length, 0);
        LOG_ERROR(
            LOG_COMPONENT,
            "handshake failed code=%d last_error=%d errno=%d message=%.*s",
            handshake_result,
            last_error,
            errno,
            error_length,
            error_message == NULL ? "" : error_message);
        set_status(SSH_STATUS_ERROR, "SSH handshake failed");
        goto cleanup;
    }
    LOG_INFO(LOG_COMPONENT, "SSH handshake complete");

    set_fingerprint(session);
    set_status(SSH_STATUS_VERIFY_HOST, "A trust / B reject");
    if (!wait_for_host_decision())
    {
        set_status(SSH_STATUS_DISCONNECTED, "Host rejected");
        goto cleanup;
    }

    set_status(SSH_STATUS_AUTHENTICATING, "Password authentication");
    LOG_INFO(LOG_COMPONENT, "password authentication starting");
    const int auth_result = libssh2_userauth_password(
        session,
        connect_command->username,
        connect_command->password);
    secure_clear(
        connect_command->password,
        sizeof(connect_command->password));
    if (auth_result != 0)
    {
        LOG_WARNING(
            LOG_COMPONENT,
            "password authentication failed code=%d",
            auth_result);
        set_status(SSH_STATUS_BAD_CREDENTIALS, "Authentication failed");
        goto cleanup;
    }
    LOG_INFO(LOG_COMPONENT, "password authentication complete");

    LOG_INFO(LOG_COMPONENT, "opening SSH session channel");
    channel = libssh2_channel_open_session(session);
    if (channel == NULL)
    {
        LOG_ERROR(LOG_COMPONENT, "channel open failed");
        set_status(SSH_STATUS_ERROR, "Cannot open shell");
        goto cleanup;
    }
    LOG_INFO(LOG_COMPONENT, "requesting terminal");
    const int pty_result = libssh2_channel_request_pty_ex(
        channel, "dumb", 4, NULL, 0, 40, 12, 0, 0);
    if (pty_result != 0)
    {
        LOG_ERROR(LOG_COMPONENT, "terminal request failed code=%d", pty_result);
        set_status(SSH_STATUS_ERROR, "Cannot open shell");
        goto cleanup;
    }
    LOG_INFO(LOG_COMPONENT, "starting remote shell");
    const int shell_result = libssh2_channel_shell(channel);
    if (shell_result != 0)
    {
        LOG_ERROR(LOG_COMPONENT, "shell start failed code=%d", shell_result);
        set_status(SSH_STATUS_ERROR, "Cannot open shell");
        goto cleanup;
    }

    /* libssh2 can enable FIONBIO but cannot query that mode, so its startup
     * path assumes lwIP sockets are already non-blocking. Set it explicitly
     * before using libssh2's non-blocking channel API. */
    int nonblocking = 1;
    if (lwip_ioctl(socket_fd, FIONBIO, &nonblocking) != 0)
    {
        LOG_ERROR(
            LOG_COMPONENT,
            "cannot enable non-blocking socket errno=%d",
            pico_lwip_errno);
        set_status(SSH_STATUS_ERROR, "Cannot configure terminal socket");
        goto cleanup;
    }
    LOG_INFO(LOG_COMPONENT, "terminal socket is non-blocking");
    libssh2_session_set_blocking(session, 0);
    set_status(SSH_STATUS_CONNECTED, connect_command->host);
    bool terminal_failed = false;
    while (!libssh2_channel_eof(channel))
    {
        bool received_data = false;
        for (unsigned int read_count = 0; read_count < 16; read_count++)
        {
            char received[128];
            const ssize_t count = libssh2_channel_read(
                channel, received, sizeof(received));
            if (count > 0)
            {
                append_terminal(received, (size_t)count);
                received_data = true;
                continue;
            }
            if (count < 0 && count != LIBSSH2_ERROR_EAGAIN)
            {
                char *error_message = NULL;
                int error_length = 0;
                const int last_error = libssh2_session_last_error(
                    session, &error_message, &error_length, 0);
                LOG_ERROR(
                    LOG_COMPONENT,
                    "terminal read failed code=%d last_error=%d errno=%d message=%.*s",
                    (int)count,
                    last_error,
                    errno,
                    error_length,
                    error_message == NULL ? "" : error_message);
                set_status(SSH_STATUS_ERROR, "Terminal read failed");
                terminal_failed = true;
            }
            break;
        }

        if (terminal_failed)
        {
            break;
        }

        SshCommand command;
        const TickType_t command_wait = received_data
            ? 0
            : pdMS_TO_TICKS(20);
        if (xQueueReceive(command_queue, &command, command_wait) == pdTRUE)
        {
            if (command.type == SSH_COMMAND_DISCONNECT)
            {
                break;
            }
            if (command.type == SSH_COMMAND_SEND_LINE)
            {
                const size_t length = strlen(command.line);
                if (!write_channel(session, channel, command.line, length) ||
                    !write_channel(session, channel, "\r", 1))
                {
                    set_status(SSH_STATUS_ERROR, "Terminal write failed");
                    terminal_failed = true;
                    break;
                }
            }
        }
    }
    if (!terminal_failed)
    {
        set_status(SSH_STATUS_DISCONNECTED, "Session closed");
    }

cleanup:
    secure_clear(
        connect_command->password,
        sizeof(connect_command->password));
    close_session(socket_fd, session, channel);
}

static void ssh_task(void *parameter)
{
    (void)parameter;
    mbedtls_platform_set_calloc_free(rtos_calloc, rtos_plain_free);
    if (libssh2_init(0) != 0)
    {
        set_status(SSH_STATUS_ERROR, "libssh2 init failed");
        vTaskDelete(NULL);
    }

    set_status(SSH_STATUS_IDLE, "Enter connection details");
    SshCommand command;
    while (xQueueReceive(command_queue, &command, portMAX_DELAY) == pdTRUE)
    {
        if (command.type == SSH_COMMAND_CONNECT)
        {
            run_connection(&command);
        }
        secure_clear(command.password, sizeof(command.password));
    }
}

#endif

bool ssh_service_start(void)
{
    command_queue = xQueueCreateStatic(
        SSH_COMMAND_QUEUE_LENGTH,
        sizeof(SshCommand),
        command_queue_bytes,
        &command_queue_storage);
    if (command_queue == NULL)
    {
        return false;
    }

#if APP_SSH_PICO_W
    return xTaskCreate(
        ssh_task,
        "ssh",
        SSH_TASK_STACK_DEPTH,
        NULL,
        SSH_TASK_PRIORITY,
        NULL) == pdPASS;
#else
    current_snapshot.status = SSH_STATUS_UNAVAILABLE;
    return true;
#endif
}

const SshServiceApi *ssh_service_api(void)
{
    return &service_api;
}
