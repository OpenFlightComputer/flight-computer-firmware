#ifndef OPENFLIGHTCOMPUTER_STATUS_INDICATOR_H
#define OPENFLIGHTCOMPUTER_STATUS_INDICATOR_H

#include "system_state.h"
#include "motor_control_internal.h"

#include <stdbool.h>

bool status_indicator_show_state(system_state_t state);
void status_indicator_motor_lifecycle_changed(
    void *context,
    motor_control_lifecycle_event_t event);
void status_indicator_process_pending(void);

#endif
