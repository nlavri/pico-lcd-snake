#include "app_registry.h"

#include "calculator_app.h"
#if APP_ENABLE_PLATFORMER
#include "platformer_demo_app.h"
#endif
#include "snake_app.h"
#include "settings_app.h"
#if APP_ENABLE_SSH
#include "ssh_app.h"
#endif
#include "temperature_app.h"
#include "weather_app.h"

const Application *const application_registry[] = {
    &snake_application,
#if APP_ENABLE_PLATFORMER
    &platformer_demo_application,
#endif
    &calculator_application,
    &temperature_application,
    &weather_application,
#if APP_ENABLE_SSH
    &ssh_application,
#endif
    &settings_application,
};

const size_t application_registry_count =
    sizeof(application_registry) / sizeof(application_registry[0]);
