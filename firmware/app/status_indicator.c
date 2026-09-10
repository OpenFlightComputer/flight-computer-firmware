#include "status_indicator.h"

#include "board.h"

#include <stdint.h>

#define STATUS_LED_BRIGHTNESS 16U

static system_state_t pending_state;
static bool pending_update;

bool status_indicator_show_state(system_state_t state)
{
    bool result = false;

    switch (state) {
    case SYSTEM_STATE_BOOT:
    case SYSTEM_STATE_ARMED:
    case SYSTEM_STATE_FAILSAFE:
    case SYSTEM_STATE_FAULT:
        result = board_status_indicator_set_rgb(0U, 0U, 0U);
        break;
    case SYSTEM_STATE_INITIALIZING:
        result = board_status_indicator_set_rgb(STATUS_LED_BRIGHTNESS,
                                                STATUS_LED_BRIGHTNESS,
                                                0U);
        break;
    case SYSTEM_STATE_DISARMED:
        result = board_status_indicator_set_rgb(0U,
                                                STATUS_LED_BRIGHTNESS,
                                                0U);
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
