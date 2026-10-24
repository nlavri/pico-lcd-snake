#ifndef PICO_LCD_LIBSSH2_PLATFORM_H
#define PICO_LCD_LIBSSH2_PLATFORM_H

/*
 * Pico W exposes the BSD socket API through lwIP rather than the desktop
 * sys/socket.h family. Pre-include the lwIP compatibility layer for every
 * upstream libssh2 translation unit so the vendor sources stay untouched.
 */
#include <fcntl.h>

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#endif
