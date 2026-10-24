#ifndef PICO_LCD_SSH_SERVICE_H
#define PICO_LCD_SSH_SERVICE_H

#include <stdbool.h>

#include "platform/ssh.h"

bool ssh_service_start(void);
const SshServiceApi *ssh_service_api(void);

#endif
