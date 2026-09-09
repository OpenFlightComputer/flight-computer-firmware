#ifndef OPENFLIGHTCOMPUTER_FLIGHT_CONFIGURATION_SERVICE_H
#define OPENFLIGHTCOMPUTER_FLIGHT_CONFIGURATION_SERVICE_H

#include "flight_configuration_storage.h"
#include "receiver_failsafe.h"
#include "receiver_service.h"
#include "system_state.h"

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t (*flight_configuration_clock_t)(void);

typedef enum {
    FLIGHT_CONFIGURATION_SOURCE_DEFAULT = 0,
    FLIGHT_CONFIGURATION_SOURCE_PERSISTENT,
} flight_configuration_source_t;

typedef enum {
    FLIGHT_CONFIGURATION_SERVICE_OK = 0,
    FLIGHT_CONFIGURATION_SERVICE_INVALID_ARGUMENT,
    FLIGHT_CONFIGURATION_SERVICE_STORAGE_ERROR,
    FLIGHT_CONFIGURATION_SERVICE_UNSAFE_STATE,
    FLIGHT_CONFIGURATION_SERVICE_APPLY_ERROR,
} flight_configuration_service_result_t;

typedef struct {
    flight_configuration_t active;
    flight_configuration_storage_t storage;
    system_state_machine_t *state_machine;
    receiver_failsafe_t *receiver_failsafe;
    receiver_service_t *receiver_service;
    flight_configuration_clock_t clock;
    flight_configuration_source_t source;
    bool initialized;
} flight_configuration_service_t;

flight_configuration_service_result_t flight_configuration_service_initialize(
    flight_configuration_service_t *service,
    const flight_configuration_storage_t *storage,
    system_state_machine_t *state_machine,
    receiver_failsafe_t *receiver_failsafe,
    receiver_service_t *receiver_service,
    flight_configuration_clock_t clock);
flight_configuration_service_result_t flight_configuration_service_write(
    flight_configuration_service_t *service,
    const flight_configuration_t *configuration);
flight_configuration_service_result_t flight_configuration_service_reset(
    flight_configuration_service_t *service);
bool flight_configuration_service_read(
    const flight_configuration_service_t *service,
    flight_configuration_t *configuration,
    flight_configuration_source_t *source);
const char *flight_configuration_source_name(
    flight_configuration_source_t source);

#endif
