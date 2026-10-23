#include "platform/network.h"

const char *network_status_name(NetworkStatus status)
{
    switch (status)
    {
        case NETWORK_STATUS_UNAVAILABLE:
            return "Unavailable";
        case NETWORK_STATUS_DISCONNECTED:
            return "Disconnected";
        case NETWORK_STATUS_SCANNING:
            return "Scanning";
        case NETWORK_STATUS_CONNECTING:
            return "Connecting";
        case NETWORK_STATUS_CONNECTED:
            return "Connected";
        case NETWORK_STATUS_BAD_PASSWORD:
            return "Bad password";
        case NETWORK_STATUS_NOT_FOUND:
            return "Not found";
        case NETWORK_STATUS_ERROR:
        default:
            return "Error";
    }
}
