#include "network_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "common/logger.h"
#include "network_defaults.h"
#include "platform/network_credentials.h"

#define LOG_COMPONENT "network"
#define NETWORK_COMMAND_QUEUE_LENGTH 3
#define NETWORK_TASK_STACK_DEPTH 2048
#define NETWORK_TASK_PRIORITY 3

typedef enum NetworkCommandType
{
    NETWORK_COMMAND_SCAN,
    NETWORK_COMMAND_CONNECT,
    NETWORK_COMMAND_FORGET,
    NETWORK_COMMAND_FETCH_WEATHER,
} NetworkCommandType;

typedef struct NetworkCommand
{
    NetworkCommandType type;
    NetworkCredentials credentials;
} NetworkCommand;

static NetworkSnapshot current_snapshot = {
    .status = NETWORK_STATUS_UNAVAILABLE,
};
static WeatherSnapshot current_weather = {
    .status = WEATHER_STATUS_UNAVAILABLE,
};
static StaticQueue_t command_queue_storage;
static uint8_t command_queue_bytes[
    NETWORK_COMMAND_QUEUE_LENGTH * sizeof(NetworkCommand)];
static QueueHandle_t command_queue;

#if APP_NETWORK_PICO_W

#include "pico/cyw43_arch.h"

#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

#define WEATHER_HOST "wttr.in"
#define WEATHER_PORT 80
#define WEATHER_RECEIVE_CAPACITY 512
#define WEATHER_LINE_CAPACITY 160
#define WEATHER_REQUEST_PATH "/?format=j1&m"

static StaticTask_t network_task_storage;
static StackType_t network_task_stack[NETWORK_TASK_STACK_DEPTH];
static NetworkCredentials active_credentials;
static bool credentials_pending_save;
static char weather_receive_buffer[WEATHER_RECEIVE_CAPACITY];

static bool load_default_credentials(NetworkCredentials *credentials)
{
    if (credentials == NULL || PICO_LCD_DEFAULT_WIFI_SSID[0] == '\0')
    {
        return false;
    }

    *credentials = (NetworkCredentials){
        .auth = PICO_LCD_DEFAULT_WIFI_PASSWORD[0] == '\0'
            ? NETWORK_AUTH_OPEN
            : NETWORK_AUTH_SECURED,
    };
    snprintf(
        credentials->ssid,
        sizeof(credentials->ssid),
        "%s",
        PICO_LCD_DEFAULT_WIFI_SSID);
    snprintf(
        credentials->password,
        sizeof(credentials->password),
        "%s",
        PICO_LCD_DEFAULT_WIFI_PASSWORD);
    return true;
}

static void weather_snapshot_changed(void)
{
    current_weather.revision++;
}

static void set_weather_status(WeatherStatus status)
{
    taskENTER_CRITICAL();
    if (current_weather.status != status)
    {
        current_weather.status = status;
        weather_snapshot_changed();
    }
    taskEXIT_CRITICAL();
}

static void snapshot_changed(void)
{
    current_snapshot.revision++;
}

static void set_status(NetworkStatus status)
{
    taskENTER_CRITICAL();
    if (current_snapshot.status != status)
    {
        current_snapshot.status = status;
        snapshot_changed();
    }
    taskEXIT_CRITICAL();
}

static void set_connected_details(void)
{
    char ip_address[NETWORK_IP_CAPACITY] = {0};
    if (netif_default != NULL)
    {
        ip4addr_ntoa_r(
            netif_ip4_addr(netif_default),
            ip_address,
            sizeof(ip_address));
    }

    taskENTER_CRITICAL();
    snprintf(
        current_snapshot.connected_ssid,
        sizeof(current_snapshot.connected_ssid),
        "%s",
        active_credentials.ssid);
    snprintf(
        current_snapshot.ip_address,
        sizeof(current_snapshot.ip_address),
        "%s",
        ip_address);
    current_snapshot.status = NETWORK_STATUS_CONNECTED;
    snapshot_changed();
    taskEXIT_CRITICAL();
}

static int scan_result_callback(
    void *environment,
    const cyw43_ev_scan_result_t *result)
{
    (void)environment;
    if (result == NULL || result->ssid_len == 0)
    {
        return 0;
    }

    char ssid[NETWORK_SSID_CAPACITY];
    const size_t length = result->ssid_len < NETWORK_SSID_CAPACITY - 1
        ? result->ssid_len
        : NETWORK_SSID_CAPACITY - 1;
    memcpy(ssid, result->ssid, length);
    ssid[length] = '\0';

    taskENTER_CRITICAL();
    size_t index = current_snapshot.access_point_count;
    for (size_t i = 0; i < current_snapshot.access_point_count; i++)
    {
        if (strcmp(current_snapshot.access_points[i].ssid, ssid) == 0)
        {
            index = i;
            break;
        }
    }

    if (index < NETWORK_SCAN_RESULT_CAPACITY)
    {
        NetworkAccessPoint *access_point =
            &current_snapshot.access_points[index];
        if (index == current_snapshot.access_point_count)
        {
            current_snapshot.access_point_count++;
        }
        if (access_point->ssid[0] == '\0' || result->rssi > access_point->rssi)
        {
            snprintf(access_point->ssid, sizeof(access_point->ssid), "%s", ssid);
            access_point->rssi = result->rssi;
            access_point->auth = result->auth_mode == CYW43_AUTH_OPEN
                ? NETWORK_AUTH_OPEN
                : NETWORK_AUTH_SECURED;
            snapshot_changed();
        }
    }
    taskEXIT_CRITICAL();
    return 0;
}

static void begin_scan(void)
{
    taskENTER_CRITICAL();
    memset(
        current_snapshot.access_points,
        0,
        sizeof(current_snapshot.access_points));
    current_snapshot.access_point_count = 0;
    current_snapshot.status = NETWORK_STATUS_SCANNING;
    snapshot_changed();
    taskEXIT_CRITICAL();

    cyw43_wifi_scan_options_t options = {0};
    const int result = cyw43_wifi_scan(
        &cyw43_state,
        &options,
        NULL,
        scan_result_callback);
    if (result != 0)
    {
        LOG_ERROR(LOG_COMPONENT, "scan start failed: %d", result);
        set_status(NETWORK_STATUS_ERROR);
    }
    else
    {
        LOG_INFO(LOG_COMPONENT, "Wi-Fi scan started");
    }
}

static void begin_connect(
    const NetworkCredentials *credentials,
    bool save_on_success)
{
    active_credentials = *credentials;
    credentials_pending_save = save_on_success;
    const uint32_t auth = credentials->auth == NETWORK_AUTH_OPEN
        ? CYW43_AUTH_OPEN
        : CYW43_AUTH_WPA2_MIXED_PSK;
    const char *password = credentials->auth == NETWORK_AUTH_OPEN
        ? NULL
        : credentials->password;

    set_status(NETWORK_STATUS_CONNECTING);
    const int result = cyw43_arch_wifi_connect_async(
        credentials->ssid,
        password,
        auth);
    if (result != 0)
    {
        LOG_ERROR(LOG_COMPONENT, "connect start failed: %d", result);
        set_status(NETWORK_STATUS_ERROR);
        return;
    }
    LOG_INFO(LOG_COMPONENT, "connecting to %s", credentials->ssid);
}

static void forget_network(void)
{
    cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    memset(&active_credentials, 0, sizeof(active_credentials));
    credentials_pending_save = false;
    const bool cleared = network_credentials_clear();

    taskENTER_CRITICAL();
    current_snapshot.connected_ssid[0] = '\0';
    current_snapshot.ip_address[0] = '\0';
    current_snapshot.status = NETWORK_STATUS_DISCONNECTED;
    snapshot_changed();
    taskEXIT_CRITICAL();

    LOG_INFO(
        LOG_COMPONENT,
        "saved network forgotten%s",
        cleared ? "" : " (flash clear failed)");
}

static void update_connection_status(void)
{
    NetworkStatus status;
    taskENTER_CRITICAL();
    status = current_snapshot.status;
    taskEXIT_CRITICAL();
    if (status != NETWORK_STATUS_CONNECTING &&
        status != NETWORK_STATUS_CONNECTED)
    {
        return;
    }

    const int link = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (link == CYW43_LINK_UP)
    {
        if (status != NETWORK_STATUS_CONNECTED)
        {
            set_connected_details();
            if (credentials_pending_save)
            {
                if (!network_credentials_save(&active_credentials))
                {
                    LOG_WARNING(LOG_COMPONENT, "failed to save Wi-Fi credentials");
                }
                credentials_pending_save = false;
            }
            LOG_INFO(
                LOG_COMPONENT,
                "connected to %s (%s)",
                active_credentials.ssid,
                current_snapshot.ip_address);
        }
    }
    else if (link == CYW43_LINK_BADAUTH)
    {
        credentials_pending_save = false;
        set_status(NETWORK_STATUS_BAD_PASSWORD);
        LOG_WARNING(LOG_COMPONENT, "Wi-Fi authentication failed");
    }
    else if (link == CYW43_LINK_NONET)
    {
        credentials_pending_save = false;
        set_status(NETWORK_STATUS_NOT_FOUND);
        LOG_WARNING(LOG_COMPONENT, "Wi-Fi network not found");
    }
    else if (link == CYW43_LINK_FAIL)
    {
        credentials_pending_save = false;
        set_status(NETWORK_STATUS_ERROR);
        LOG_WARNING(LOG_COMPONENT, "Wi-Fi connection failed");
    }
    else if (status == NETWORK_STATUS_CONNECTED && link == CYW43_LINK_DOWN)
    {
        set_status(NETWORK_STATUS_DISCONNECTED);
        LOG_WARNING(LOG_COMPONENT, "Wi-Fi disconnected");
    }
}

static void copy_ascii_field(char *destination, size_t capacity, char *source)
{
    size_t output = 0;
    while (*source != '\0' && output + 1 < capacity)
    {
        const unsigned char value = (unsigned char)*source++;
        if (value >= 32 && value <= 126)
        {
            destination[output++] = (char)value;
        }
    }
    while (output > 0 && destination[output - 1] == ' ')
    {
        output--;
    }
    destination[output] = '\0';
}

static const char *json_line_value(const char *line, const char *key)
{
    char pattern[40];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *position = strstr(line, pattern);
    if (position == NULL)
    {
        return NULL;
    }
    position = strchr(position + strlen(pattern), ':');
    if (position == NULL)
    {
        return NULL;
    }
    position++;
    while (*position == ' ' || *position == '\t' ||
           *position == '\r' || *position == '\n')
    {
        position++;
    }
    return position;
}

static bool json_line_string(
    const char *line,
    const char *key,
    char *destination,
    size_t capacity)
{
    const char *value = json_line_value(line, key);
    if (value == NULL || *value != '\"')
    {
        return false;
    }
    value++;
    const char *end = strchr(value, '\"');
    if (end == NULL)
    {
        return false;
    }
    char field[WEATHER_LOCATION_CAPACITY];
    const size_t length = (size_t)(end - value) < sizeof(field) - 1
        ? (size_t)(end - value)
        : sizeof(field) - 1;
    memcpy(field, value, length);
    field[length] = '\0';
    copy_ascii_field(destination, capacity, field);
    return destination[0] != '\0';
}

static bool json_line_integer(
    const char *line,
    const char *key,
    long *result)
{
    const char *value = json_line_value(line, key);
    if (value == NULL)
    {
        return false;
    }
    if (*value == '\"')
    {
        value++;
    }
    char *end;
    *result = strtol(value, &end, 10);
    return end != value;
}

static bool json_line_tenths(
    const char *line,
    const char *key,
    uint16_t *result)
{
    const char *value = json_line_value(line, key);
    if (value == NULL)
    {
        return false;
    }
    if (*value == '"')
    {
        value++;
    }
    char *end;
    const unsigned long whole = strtoul(value, &end, 10);
    if (end == value || whole > 6553)
    {
        return false;
    }
    uint8_t decimal = 0;
    if (*end == '.' && end[1] >= '0' && end[1] <= '9')
    {
        decimal = (uint8_t)(end[1] - '0');
    }
    *result = (uint16_t)(whole * 10 + decimal);
    return true;
}

typedef struct WeatherStreamParser
{
    WeatherSnapshot *weather;
    int forecast_index;
    long pending_rain_chance;
    bool in_forecast;
    bool awaiting_location;
    bool awaiting_description;
    bool description_is_forecast;
    bool capture_noon;
    bool has_temperature;
    bool has_condition;
    bool has_location;
} WeatherStreamParser;

static void parse_weather_line(WeatherStreamParser *parser, const char *line)
{
    WeatherSnapshot *weather = parser->weather;
    long value;

    if (strstr(line, "\"weather\": [") != NULL)
    {
        parser->in_forecast = true;
        return;
    }

    if (!parser->in_forecast)
    {
        if (strstr(line, "\"areaName\"") != NULL)
        {
            parser->awaiting_location = true;
            return;
        }
        if (strstr(line, "\"weatherDesc\"") != NULL)
        {
            parser->awaiting_description = true;
            parser->description_is_forecast = false;
            return;
        }
        if (parser->awaiting_location &&
            json_line_string(
                line,
                "value",
                weather->location,
                sizeof(weather->location)))
        {
            parser->awaiting_location = false;
            parser->has_location = true;
            return;
        }
        if (parser->awaiting_description &&
            json_line_string(
                line,
                "value",
                weather->condition,
                sizeof(weather->condition)))
        {
            parser->awaiting_description = false;
            parser->has_condition = true;
            return;
        }
        if (json_line_integer(line, "temp_C", &value))
        {
            weather->temperature_c = (int16_t)value;
            parser->has_temperature = true;
        }
        else if (json_line_integer(line, "FeelsLikeC", &value))
        {
            weather->feels_like_c = (int16_t)value;
        }
        else if (json_line_integer(line, "humidity", &value) &&
                 value >= 0 && value <= 100)
        {
            weather->humidity_percent = (uint8_t)value;
        }
        else if (json_line_integer(line, "weatherCode", &value))
        {
            weather->condition_code = (uint16_t)value;
        }
        else if (json_line_integer(line, "windspeedKmph", &value))
        {
            weather->wind_speed_kmph = (uint16_t)value;
        }
        else if (json_line_integer(line, "visibility", &value) &&
                 value >= 0 && value <= UINT8_MAX)
        {
            weather->visibility_km = (uint8_t)value;
        }
        else if (json_line_tenths(
                     line,
                     "precipMM",
                     &weather->precipitation_tenths_mm))
        {
        }
        else
        {
            json_line_string(
                line,
                "winddir16Point",
                weather->wind_direction,
                sizeof(weather->wind_direction));
        }
        return;
    }

    char forecast_date[sizeof(weather->forecast[0].date)];
    if (json_line_string(
            line,
            "date",
            forecast_date,
            sizeof(forecast_date)))
    {
        if (weather->forecast_count < WEATHER_FORECAST_DAY_CAPACITY)
        {
            parser->forecast_index = weather->forecast_count++;
            snprintf(
                weather->forecast[parser->forecast_index].date,
                sizeof(weather->forecast[0].date),
                "%s",
                forecast_date);
            parser->capture_noon = false;
        }
        return;
    }
    if (parser->forecast_index < 0 ||
        parser->forecast_index >= weather->forecast_count)
    {
        return;
    }

    WeatherForecastDay *day = &weather->forecast[parser->forecast_index];
    if (json_line_integer(line, "chanceofrain", &value))
    {
        parser->pending_rain_chance = value;
    }
    else if (json_line_integer(line, "time", &value))
    {
        parser->capture_noon = value == 1200;
        if (parser->capture_noon && parser->pending_rain_chance >= 0 &&
            parser->pending_rain_chance <= 100)
        {
            day->rain_chance_percent =
                (uint8_t)parser->pending_rain_chance;
        }
    }
    else if (json_line_integer(line, "maxtempC", &value))
    {
        day->maximum_c = (int16_t)value;
    }
    else if (json_line_integer(line, "mintempC", &value))
    {
        day->minimum_c = (int16_t)value;
    }
    else if (parser->capture_noon &&
             json_line_integer(line, "weatherCode", &value))
    {
        day->condition_code = (uint16_t)value;
    }
    else if (parser->capture_noon &&
             json_line_integer(line, "windspeedKmph", &value))
    {
        day->wind_speed_kmph = (uint16_t)value;
    }
    else if (parser->capture_noon &&
             json_line_string(
                 line,
                 "winddir16Point",
                 day->wind_direction,
                 sizeof(day->wind_direction)))
    {
    }
    else if (parser->capture_noon &&
             strstr(line, "\"weatherDesc\"") != NULL)
    {
        parser->awaiting_description = true;
        parser->description_is_forecast = true;
    }
    else if (parser->awaiting_description &&
             parser->description_is_forecast &&
             json_line_string(
                 line,
                 "value",
                 day->condition,
                 sizeof(day->condition)))
    {
        parser->awaiting_description = false;
    }
}

static bool receive_weather_response(int socket_fd, WeatherSnapshot *weather)
{
    *weather = (WeatherSnapshot){
        .status = WEATHER_STATUS_READY,
    };
    WeatherStreamParser parser = {
        .weather = weather,
        .forecast_index = -1,
        .pending_rain_chance = -1,
    };
    char line[WEATHER_LINE_CAPACITY];
    size_t line_length = 0;
    bool headers_complete = false;
    bool status_ok = false;

    for (;;)
    {
        const int received = lwip_recv(
            socket_fd,
            weather_receive_buffer,
            sizeof(weather_receive_buffer),
            0);
        if (received < 0)
        {
            return false;
        }
        if (received == 0)
        {
            break;
        }
        for (int i = 0; i < received; i++)
        {
            const char character = weather_receive_buffer[i];
            if (character != '\n')
            {
                if (character != '\r' &&
                    line_length + 1 < sizeof(line))
                {
                    line[line_length++] = character;
                }
                continue;
            }

            line[line_length] = '\0';
            if (!headers_complete)
            {
                if (strncmp(line, "HTTP/1.0 200", 12) == 0 ||
                    strncmp(line, "HTTP/1.1 200", 12) == 0)
                {
                    status_ok = true;
                }
                if (line_length == 0)
                {
                    headers_complete = true;
                }
            }
            else
            {
                parse_weather_line(&parser, line);
            }
            line_length = 0;
        }
    }
    if (headers_complete && line_length > 0)
    {
        line[line_length] = '\0';
        parse_weather_line(&parser, line);
    }
    return status_ok && headers_complete && parser.has_temperature &&
        parser.has_condition && parser.has_location &&
        weather->forecast_count >= 2;
}

static bool fetch_weather(WeatherSnapshot *weather)
{
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *address = NULL;
    if (lwip_getaddrinfo(WEATHER_HOST, "80", &hints, &address) != 0 ||
        address == NULL)
    {
        return false;
    }

    const int socket_fd = lwip_socket(
        address->ai_family,
        address->ai_socktype,
        address->ai_protocol);
    if (socket_fd < 0)
    {
        lwip_freeaddrinfo(address);
        return false;
    }

    const struct timeval timeout = {
        .tv_sec = 10,
        .tv_usec = 0,
    };
    lwip_setsockopt(
        socket_fd,
        SOL_SOCKET,
        SO_RCVTIMEO,
        &timeout,
        sizeof(timeout));
    lwip_setsockopt(
        socket_fd,
        SOL_SOCKET,
        SO_SNDTIMEO,
        &timeout,
        sizeof(timeout));

    bool success = false;
    if (lwip_connect(socket_fd, address->ai_addr, address->ai_addrlen) == 0)
    {
        static const char request[] =
            "GET " WEATHER_REQUEST_PATH " HTTP/1.0\r\n"
            "Host: " WEATHER_HOST "\r\n"
            "User-Agent: pico-lcd-weather/0.1\r\n"
            "Accept: text/plain\r\n"
            "Connection: close\r\n\r\n";
        const int sent = lwip_send(
            socket_fd,
            request,
            sizeof(request) - 1,
            0);
        success = sent == (int)(sizeof(request) - 1) &&
            receive_weather_response(socket_fd, weather);
    }

    lwip_close(socket_fd);
    lwip_freeaddrinfo(address);
    return success;
}

static void refresh_weather(void)
{
    if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) !=
        CYW43_LINK_UP)
    {
        set_weather_status(WEATHER_STATUS_OFFLINE);
        return;
    }

    set_weather_status(WEATHER_STATUS_LOADING);
    LOG_INFO(LOG_COMPONENT, "requesting weather for IP-based location");
    WeatherSnapshot weather;
    if (!fetch_weather(&weather))
    {
        set_weather_status(WEATHER_STATUS_ERROR);
        LOG_WARNING(LOG_COMPONENT, "weather request failed");
        return;
    }

    taskENTER_CRITICAL();
    weather.revision = current_weather.revision + 1;
    current_weather = weather;
    taskEXIT_CRITICAL();
    LOG_INFO(
        LOG_COMPONENT,
        "weather: %s, %d C, %s",
        current_weather.location,
        current_weather.temperature_c,
        current_weather.condition);
}

static void handle_command(const NetworkCommand *command)
{
    switch (command->type)
    {
        case NETWORK_COMMAND_SCAN:
            begin_scan();
            break;
        case NETWORK_COMMAND_CONNECT:
            begin_connect(&command->credentials, true);
            break;
        case NETWORK_COMMAND_FORGET:
            forget_network();
            break;
        case NETWORK_COMMAND_FETCH_WEATHER:
            refresh_weather();
            break;
    }
}

static void network_task(void *parameter)
{
    (void)parameter;
    if (cyw43_arch_init() != 0)
    {
        LOG_ERROR(LOG_COMPONENT, "CYW43 initialization failed");
        set_status(NETWORK_STATUS_ERROR);
        vTaskDelete(NULL);
    }

    cyw43_arch_enable_sta_mode();
    set_status(NETWORK_STATUS_DISCONNECTED);
    set_weather_status(WEATHER_STATUS_IDLE);
    LOG_INFO(LOG_COMPONENT, "Pico W network service ready");

    NetworkCredentials saved_credentials;
    if (network_credentials_load(&saved_credentials))
    {
        begin_connect(&saved_credentials, false);
    }
    else if (load_default_credentials(&saved_credentials))
    {
        LOG_INFO(LOG_COMPONENT, "using local default Wi-Fi network");
        begin_connect(&saved_credentials, true);
    }

    while (true)
    {
        NetworkCommand command;
        if (xQueueReceive(
                command_queue,
                &command,
                pdMS_TO_TICKS(100)) == pdTRUE)
        {
            handle_command(&command);
        }

        if (current_snapshot.status == NETWORK_STATUS_SCANNING &&
            !cyw43_wifi_scan_active(&cyw43_state))
        {
            if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) ==
                CYW43_LINK_UP)
            {
                set_connected_details();
            }
            else
            {
                set_status(NETWORK_STATUS_DISCONNECTED);
            }
            LOG_INFO(
                LOG_COMPONENT,
                "Wi-Fi scan complete: %u network(s)",
                (unsigned)current_snapshot.access_point_count);
        }
        update_connection_status();
    }
}

#endif

static void get_snapshot(NetworkSnapshot *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }
#if APP_NETWORK_PICO_W
    taskENTER_CRITICAL();
#endif
    *snapshot = current_snapshot;
#if APP_NETWORK_PICO_W
    taskEXIT_CRITICAL();
#endif
}

static bool queue_command(const NetworkCommand *command)
{
    return command_queue != NULL &&
        xQueueSend(command_queue, command, 0) == pdTRUE;
}

static bool request_scan(void)
{
    const NetworkCommand command = {.type = NETWORK_COMMAND_SCAN};
    return queue_command(&command);
}

static bool request_connect(
    const char *ssid,
    const char *password,
    NetworkAuth auth)
{
    const size_t password_length = password == NULL ? 0 : strlen(password);
    if (ssid == NULL || ssid[0] == '\0' ||
        strlen(ssid) >= NETWORK_SSID_CAPACITY ||
        password == NULL || password_length >= NETWORK_PASSWORD_CAPACITY ||
        (auth == NETWORK_AUTH_SECURED && password_length < 8))
    {
        return false;
    }

    NetworkCommand command = {
        .type = NETWORK_COMMAND_CONNECT,
        .credentials.auth = auth,
    };
    snprintf(
        command.credentials.ssid,
        sizeof(command.credentials.ssid),
        "%s",
        ssid);
    snprintf(
        command.credentials.password,
        sizeof(command.credentials.password),
        "%s",
        password);
    return queue_command(&command);
}

static bool request_forget(void)
{
    const NetworkCommand command = {.type = NETWORK_COMMAND_FORGET};
    return queue_command(&command);
}

static void get_weather_snapshot(WeatherSnapshot *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }
#if APP_NETWORK_PICO_W
    taskENTER_CRITICAL();
#endif
    *snapshot = current_weather;
#if APP_NETWORK_PICO_W
    taskEXIT_CRITICAL();
#endif
}

static bool request_weather_refresh(void)
{
    const NetworkCommand command = {
        .type = NETWORK_COMMAND_FETCH_WEATHER,
    };
    const bool queued = queue_command(&command);
#if APP_NETWORK_PICO_W
    if (queued)
    {
        set_weather_status(WEATHER_STATUS_LOADING);
        LOG_INFO(LOG_COMPONENT, "weather refresh queued");
    }
    else
    {
        LOG_WARNING(LOG_COMPONENT, "weather refresh queue full");
    }
#endif
    return queued;
}

static const NetworkServiceApi service_api = {
    .get_snapshot = get_snapshot,
    .request_scan = request_scan,
    .request_connect = request_connect,
    .request_forget = request_forget,
};

static const WeatherServiceApi weather_api = {
    .get_snapshot = get_weather_snapshot,
    .request_refresh = request_weather_refresh,
};

bool network_service_start(void)
{
#if APP_NETWORK_PICO_W
    command_queue = xQueueCreateStatic(
        NETWORK_COMMAND_QUEUE_LENGTH,
        sizeof(NetworkCommand),
        command_queue_bytes,
        &command_queue_storage);
    if (command_queue == NULL)
    {
        return false;
    }

    return xTaskCreateStatic(
        network_task,
        "network",
        NETWORK_TASK_STACK_DEPTH,
        NULL,
        NETWORK_TASK_PRIORITY,
        network_task_stack,
        &network_task_storage) != NULL;
#else
    return true;
#endif
}

const NetworkServiceApi *network_service_api(void)
{
    return &service_api;
}

const WeatherServiceApi *weather_service_api(void)
{
    return &weather_api;
}
