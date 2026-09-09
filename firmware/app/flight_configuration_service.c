#include "flight_configuration_service.h"

#include "motor_control.h"

#include <stddef.h>

static bool storage_is_valid(const flight_configuration_storage_t *storage)
{
    return (storage != NULL) && (storage->load != NULL) &&
           (storage->save != NULL) && (storage->clear != NULL);
}

static bool runtime_is_safe(const flight_configuration_service_t *service)
{
    return (service->state_machine->current == SYSTEM_STATE_DISARMED) &&
           (motor_control_pending_source() == MOTOR_CONTROL_SOURCE_NONE);
}

static bool apply_runtime(flight_configuration_service_t *service,
                          const flight_configuration_t *configuration)
{
    const receiver_freshness_config_t freshness = {
        .fresh_through_us = configuration->receiver_failsafe.stale_after_us,
        .lost_after_us =
            configuration->receiver_failsafe.loss_detected_after_us,
    };

    if (motor_control_apply_configuration(&configuration->motors) !=
        MOTOR_CONTROL_CONFIGURATION_APPLY_OK) {
        return false;
    }
    if (!service->receiver_service->initialized) {
        return true;
    }
    return receiver_service_update_freshness_config(
               service->receiver_service, &freshness) &&
           receiver_failsafe_initialize(service->receiver_failsafe,
                                        &configuration->receiver_failsafe,
                                        service->clock());
}

flight_configuration_service_result_t flight_configuration_service_initialize(
    flight_configuration_service_t *service,
    const flight_configuration_storage_t *storage,
    system_state_machine_t *state_machine,
    receiver_failsafe_t *receiver_failsafe,
    receiver_service_t *receiver_service,
    flight_configuration_clock_t clock)
{
    flight_configuration_load_result_t load_result;

    if ((service == NULL) || !storage_is_valid(storage) ||
        (state_machine == NULL) || !state_machine->initialized ||
        (receiver_failsafe == NULL) || (receiver_service == NULL) ||
        (clock == NULL)) {
        return FLIGHT_CONFIGURATION_SERVICE_INVALID_ARGUMENT;
    }

    *service = (flight_configuration_service_t){
        .storage = *storage,
        .state_machine = state_machine,
        .receiver_failsafe = receiver_failsafe,
        .receiver_service = receiver_service,
        .clock = clock,
    };
    flight_configuration_defaults(&service->active);
    load_result = service->storage.load(service->storage.context,
                                        &service->active);
    if (load_result == FLIGHT_CONFIGURATION_LOAD_OK) {
        if (!flight_configuration_is_valid(&service->active)) {
            return FLIGHT_CONFIGURATION_SERVICE_STORAGE_ERROR;
        }
        service->source = FLIGHT_CONFIGURATION_SOURCE_PERSISTENT;
    } else if (load_result == FLIGHT_CONFIGURATION_LOAD_EMPTY) {
        service->source = FLIGHT_CONFIGURATION_SOURCE_DEFAULT;
    } else {
        return FLIGHT_CONFIGURATION_SERVICE_STORAGE_ERROR;
    }
    service->initialized = true;
    return FLIGHT_CONFIGURATION_SERVICE_OK;
}

flight_configuration_service_result_t flight_configuration_service_write(
    flight_configuration_service_t *service,
    const flight_configuration_t *configuration)
{
    if ((service == NULL) || !service->initialized ||
        !flight_configuration_is_valid(configuration)) {
        return FLIGHT_CONFIGURATION_SERVICE_INVALID_ARGUMENT;
    }
    if (!runtime_is_safe(service)) {
        return FLIGHT_CONFIGURATION_SERVICE_UNSAFE_STATE;
    }
    if (service->storage.save(service->storage.context, configuration) !=
        FLIGHT_CONFIGURATION_SAVE_OK) {
        return FLIGHT_CONFIGURATION_SERVICE_STORAGE_ERROR;
    }
    if (!apply_runtime(service, configuration)) {
        return FLIGHT_CONFIGURATION_SERVICE_APPLY_ERROR;
    }
    service->active = *configuration;
    service->source = FLIGHT_CONFIGURATION_SOURCE_PERSISTENT;
    return FLIGHT_CONFIGURATION_SERVICE_OK;
}

flight_configuration_service_result_t flight_configuration_service_reset(
    flight_configuration_service_t *service)
{
    flight_configuration_t defaults;

    if ((service == NULL) || !service->initialized) {
        return FLIGHT_CONFIGURATION_SERVICE_INVALID_ARGUMENT;
    }
    if (!runtime_is_safe(service)) {
        return FLIGHT_CONFIGURATION_SERVICE_UNSAFE_STATE;
    }
    flight_configuration_defaults(&defaults);
    if (service->storage.clear(service->storage.context) !=
        FLIGHT_CONFIGURATION_CLEAR_OK) {
        return FLIGHT_CONFIGURATION_SERVICE_STORAGE_ERROR;
    }
    if (!apply_runtime(service, &defaults)) {
        return FLIGHT_CONFIGURATION_SERVICE_APPLY_ERROR;
    }
    service->active = defaults;
    service->source = FLIGHT_CONFIGURATION_SOURCE_DEFAULT;
    return FLIGHT_CONFIGURATION_SERVICE_OK;
}

bool flight_configuration_service_read(
    const flight_configuration_service_t *service,
    flight_configuration_t *configuration,
    flight_configuration_source_t *source)
{
    if ((service == NULL) || !service->initialized ||
        (configuration == NULL) || (source == NULL)) {
        return false;
    }
    *configuration = service->active;
    *source = service->source;
    return true;
}

const char *flight_configuration_source_name(
    flight_configuration_source_t source)
{
    switch (source) {
    case FLIGHT_CONFIGURATION_SOURCE_DEFAULT:
        return "DEFAULT";
    case FLIGHT_CONFIGURATION_SOURCE_PERSISTENT:
        return "PERSISTENT";
    }
    return "INVALID";
}
