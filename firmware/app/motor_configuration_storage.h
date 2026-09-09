#ifndef OPENFLIGHTCOMPUTER_MOTOR_CONFIGURATION_STORAGE_H
#define OPENFLIGHTCOMPUTER_MOTOR_CONFIGURATION_STORAGE_H

#include "motor_configuration.h"

typedef enum {
    MOTOR_CONFIGURATION_LOAD_OK = 0,
    MOTOR_CONFIGURATION_LOAD_EMPTY,
    MOTOR_CONFIGURATION_LOAD_ERROR,
} motor_configuration_load_result_t;

typedef enum {
    MOTOR_CONFIGURATION_SAVE_OK = 0,
    MOTOR_CONFIGURATION_SAVE_ERROR,
} motor_configuration_save_result_t;

typedef enum {
    MOTOR_CONFIGURATION_CLEAR_OK = 0,
    MOTOR_CONFIGURATION_CLEAR_ERROR,
} motor_configuration_clear_result_t;

typedef motor_configuration_load_result_t
    (*motor_configuration_load_t)(void *context,
                                  motor_configuration_t *configuration);
typedef motor_configuration_save_result_t
    (*motor_configuration_save_t)(void *context,
                                  const motor_configuration_t *configuration);
typedef motor_configuration_clear_result_t
    (*motor_configuration_clear_t)(void *context);

typedef struct {
    motor_configuration_load_t load;
    motor_configuration_save_t save;
    motor_configuration_clear_t clear;
    void *context;
} motor_configuration_storage_t;

#endif
