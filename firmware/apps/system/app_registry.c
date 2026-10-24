#include "app_registry.h"

#include "calculator_app.h"
#include "snake_app.h"
#include "settings_app.h"
#include "ssh_app.h"
#include "temperature_app.h"
#include "weather_app.h"

const Application *const application_registry[] = {
    &snake_application,
    &calculator_application,
    &temperature_application,
    &weather_application,
    &ssh_application,
    &settings_application,
};

const size_t application_registry_count =
    sizeof(application_registry) / sizeof(application_registry[0]);
