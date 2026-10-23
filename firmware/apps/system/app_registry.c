#include "app_registry.h"

#include "snake_app.h"
#include "temperature_app.h"

const Application *const application_registry[] = {
    &snake_application,
    &temperature_application,
};

const size_t application_registry_count =
    sizeof(application_registry) / sizeof(application_registry[0]);
