#include "flight_behavior_configuration.h"

#include <stddef.h>

bool flight_behavior_configuration_is_valid(
    const flight_behavior_configuration_t *configuration)
{
    if (configuration == NULL) {
        return false;
    }
    switch (configuration->id) {
    case FLIGHT_BEHAVIOR_MANUAL_EASY:
        return configuration->settings.manual_easy.reserved == 0U;
    case FLIGHT_BEHAVIOR_COUNT:
        break;
    }
    return false;
}
