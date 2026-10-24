#ifndef PICO_LCD_SSH_DEFAULTS_H
#define PICO_LCD_SSH_DEFAULTS_H

#if defined(__has_include)
#if __has_include("ssh_defaults.local.h")
#include "ssh_defaults.local.h"
#endif
#endif

#ifndef PICO_LCD_DEFAULT_SSH_HOST
#define PICO_LCD_DEFAULT_SSH_HOST "192.168.100.7"
#endif

#ifndef PICO_LCD_DEFAULT_SSH_USERNAME
#define PICO_LCD_DEFAULT_SSH_USERNAME ""
#endif

#ifndef PICO_LCD_DEFAULT_SSH_PASSWORD
#define PICO_LCD_DEFAULT_SSH_PASSWORD ""
#endif

#endif
