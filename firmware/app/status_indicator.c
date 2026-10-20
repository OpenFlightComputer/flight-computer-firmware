#include "status_indicator.h"

#include <stdint.h>

#define STATUS_LED_BRIGHTNESS 16U

static system_state_t pending_state;
static bool pending_update;
static status_indicator_backend_t indicator_backend;

void status_indicator_attach_backend(status_indicator_backend_t backend)
{
    indicator_backend = backend;
}

bool status_indicator_show_state(system_state_t state)
{
    bool result = false;

    if (indicator_backend == NULL) {
        pending_update = false;
        return state < SYSTEM_STATE_COUNT;
    }

    switch (state) {
    case SYSTEM_STATE_BOOT:
    case SYSTEM_STATE_ARMED:
    case SYSTEM_STATE_FAILSAFE:
    case SYSTEM_STATE_FAULT:
        result = indicator_backend(0U, 0U, 0U);
        break;
    case SYSTEM_STATE_INITIALIZING:
        result = indicator_backend(STATUS_LED_BRIGHTNESS,
                                   STATUS_LED_BRIGHTNESS,
                                   0U);
        break;
    case SYSTEM_STATE_DISARMED:
        result = indicator_backend(0U, STATUS_LED_BRIGHTNESS, 0U);
        break;
    case SYSTEM_STATE_COUNT:
        return false;
    }
    if (result) {
        pending_update = false;
    }
    return result;
}

void status_indicator_motor_lifecycle_changed(
    void *context,
    motor_control_lifecycle_event_t event)
{
    (void)context;
    if (event == MOTOR_CONTROL_LIFECYCLE_ARM_PREPARATION_STARTED) {
        pending_update = false;
        (void)status_indicator_show_state(SYSTEM_STATE_ARMED);
        return;
    }
    if (event == MOTOR_CONTROL_LIFECYCLE_DISARMED) {
        pending_state = SYSTEM_STATE_DISARMED;
        pending_update = true;
    }
}

void status_indicator_process_pending(void)
{
    if (pending_update) {
        (void)status_indicator_show_state(pending_state);
    }
}
