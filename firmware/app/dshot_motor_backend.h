#ifndef OPENFLIGHTCOMPUTER_DSHOT_MOTOR_BACKEND_H
#define OPENFLIGHTCOMPUTER_DSHOT_MOTOR_BACKEND_H

#include "dshot_timing.h"
#include "motor_output.h"

#include <stdbool.h>

typedef struct {
    dshot_timing_profile_t timing;
    dshot_dma_buffer_t stop_buffer;
    bool initialized;
    bool prepared;
} dshot_motor_backend_t;

bool dshot_motor_backend_prepare(dshot_motor_backend_t *backend,
                                 motor_output_backend_t *output_backend);

#endif
