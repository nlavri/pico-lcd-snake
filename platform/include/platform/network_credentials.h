#ifndef PICO_LCD_PLATFORM_NETWORK_CREDENTIALS_H
#define PICO_LCD_PLATFORM_NETWORK_CREDENTIALS_H

#include <stdbool.h>

#include "platform/network.h"

typedef struct NetworkCredentials
{
    char ssid[NETWORK_SSID_CAPACITY];
    char password[NETWORK_PASSWORD_CAPACITY];
    NetworkAuth auth;
} NetworkCredentials;

bool network_credentials_load(NetworkCredentials *credentials);
bool network_credentials_save(const NetworkCredentials *credentials);
bool network_credentials_clear(void);

#endif
