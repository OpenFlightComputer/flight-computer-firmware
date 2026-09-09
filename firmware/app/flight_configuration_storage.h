#ifndef OPENFLIGHTCOMPUTER_FLIGHT_CONFIGURATION_STORAGE_H
#define OPENFLIGHTCOMPUTER_FLIGHT_CONFIGURATION_STORAGE_H

#include "flight_configuration.h"

typedef enum {
    FLIGHT_CONFIGURATION_LOAD_OK = 0,
    FLIGHT_CONFIGURATION_LOAD_EMPTY,
    FLIGHT_CONFIGURATION_LOAD_ERROR,
} flight_configuration_load_result_t;

typedef enum {
    FLIGHT_CONFIGURATION_SAVE_OK = 0,
    FLIGHT_CONFIGURATION_SAVE_ERROR,
} flight_configuration_save_result_t;

typedef enum {
    FLIGHT_CONFIGURATION_CLEAR_OK = 0,
    FLIGHT_CONFIGURATION_CLEAR_ERROR,
} flight_configuration_clear_result_t;

typedef struct {
    flight_configuration_load_result_t (*load)(
        void *context,
        flight_configuration_t *configuration);
    flight_configuration_save_result_t (*save)(
        void *context,
        const flight_configuration_t *configuration);
    flight_configuration_clear_result_t (*clear)(void *context);
    void *context;
} flight_configuration_storage_t;

#endif
