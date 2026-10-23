#ifndef PICO_LCD_PLATFORM_NETWORK_H
#define PICO_LCD_PLATFORM_NETWORK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NETWORK_SSID_CAPACITY 33
#define NETWORK_PASSWORD_CAPACITY 65
#define NETWORK_IP_CAPACITY 16
#define NETWORK_SCAN_RESULT_CAPACITY 8

typedef enum NetworkAuth
{
    NETWORK_AUTH_OPEN,
    NETWORK_AUTH_SECURED,
} NetworkAuth;

typedef enum NetworkStatus
{
    NETWORK_STATUS_UNAVAILABLE,
    NETWORK_STATUS_DISCONNECTED,
    NETWORK_STATUS_SCANNING,
    NETWORK_STATUS_CONNECTING,
    NETWORK_STATUS_CONNECTED,
    NETWORK_STATUS_BAD_PASSWORD,
    NETWORK_STATUS_NOT_FOUND,
    NETWORK_STATUS_ERROR,
} NetworkStatus;

typedef struct NetworkAccessPoint
{
    char ssid[NETWORK_SSID_CAPACITY];
    NetworkAuth auth;
    int16_t rssi;
} NetworkAccessPoint;

typedef struct NetworkSnapshot
{
    NetworkStatus status;
    uint32_t revision;
    char connected_ssid[NETWORK_SSID_CAPACITY];
    char ip_address[NETWORK_IP_CAPACITY];
    NetworkAccessPoint access_points[NETWORK_SCAN_RESULT_CAPACITY];
    size_t access_point_count;
} NetworkSnapshot;

typedef struct NetworkServiceApi
{
    void (*get_snapshot)(NetworkSnapshot *snapshot);
    bool (*request_scan)(void);
    bool (*request_connect)(
        const char *ssid,
        const char *password,
        NetworkAuth auth);
    bool (*request_forget)(void);
} NetworkServiceApi;

const char *network_status_name(NetworkStatus status);

#endif
