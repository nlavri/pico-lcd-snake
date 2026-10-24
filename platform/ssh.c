#include "platform/ssh.h"

const char *ssh_status_name(SshStatus status)
{
    switch (status)
    {
        case SSH_STATUS_UNAVAILABLE: return "Unavailable";
        case SSH_STATUS_IDLE: return "Ready";
        case SSH_STATUS_CONNECTING: return "Connecting";
        case SSH_STATUS_VERIFY_HOST: return "Verify host";
        case SSH_STATUS_AUTHENTICATING: return "Authenticating";
        case SSH_STATUS_CONNECTED: return "Connected";
        case SSH_STATUS_BAD_CREDENTIALS: return "Bad credentials";
        case SSH_STATUS_DISCONNECTED: return "Disconnected";
        case SSH_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}
