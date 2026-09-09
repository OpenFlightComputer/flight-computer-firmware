#include "flight_control.h"

#include "motor_control.h"

#include <assert.h>
#include <math.h>
#include <string.h>

static motor_control_source_t active_source;
static motor_control_submit_result_t submit_result;
static motor_control_failsafe_result_t failsafe_result;
static motor_command_t submitted_command;
static uint32_t submit_count;
static uint32_t failsafe_count;
static uint32_t recovery_count;

motor_control_source_t motor_control_active_source(void)
{
    return active_source;
}

motor_control_submit_result_t motor_control_submit(
    motor_control_source_t source,
    const motor_command_t *command)
{
    assert(source == MOTOR_CONTROL_SOURCE_RECEIVER);
    assert(command != NULL);
    submit_count++;
    submitted_command = *command;
    return submit_result;
}

motor_control_failsafe_result_t motor_control_enter_failsafe(void)
{
    failsafe_count++;
    return failsafe_result;
}

motor_control_recovery_result_t motor_control_recover_to_disarmed(void)
{
    recovery_count++;
    return MOTOR_CONTROL_RECOVERY_ACCEPTED;
}

bool receiver_failsafe_release_stage_two(receiver_failsafe_t *failsafe)
{
    if ((failsafe == NULL) || !failsafe->recovery_ready) {
        return false;
    }
    failsafe->stage_two_latched = false;
    failsafe->recovery_ready = false;
    return true;
}

static flight_configuration_t configuration(void)
{
    return (flight_configuration_t){
        .schema_version = 1U,
        .propeller_layout = PROPELLER_LAYOUT_PROPS_IN,
        .mixer = {
            .roll_factor = 0.25F,
            .pitch_factor = 0.25F,
            .yaw_factor = 0.15F,
        },
    };
}

static receiver_failsafe_decision_t live_decision(void)
{
    return (receiver_failsafe_decision_t){
        .action = RECEIVER_FAILSAFE_ACTION_LIVE,
        .requested_control = {
            .roll = 0.4F,
            .pitch = -0.2F,
            .yaw = 0.5F,
            .throttle = 0.5F,
            .valid = true,
        },
    };
}

int main(void)
{
    flight_configuration_t config = configuration();
    receiver_failsafe_decision_t decision = live_decision();

    active_source = MOTOR_CONTROL_SOURCE_NONE;
    assert(flight_control_process_receiver(&config, &decision, 42U) ==
           FLIGHT_CONTROL_IDLE);
    assert(submit_count == 0U);

    active_source = MOTOR_CONTROL_SOURCE_RECEIVER;
    submit_result = MOTOR_CONTROL_SUBMIT_ACCEPTED;
    assert(flight_control_process_receiver(&config, &decision, 42U) ==
           FLIGHT_CONTROL_SUBMITTED);
    assert(submit_count == 1U);
    assert(submitted_command.valid);
    assert(submitted_command.timestamp_us == 42U);
    assert(fabsf(submitted_command.throttle[0] - 0.725F) < 0.000001F);
    assert(fabsf(submitted_command.throttle[1] - 0.475F) < 0.000001F);
    assert(fabsf(submitted_command.throttle[2] - 0.375F) < 0.000001F);
    assert(fabsf(submitted_command.throttle[3] - 0.425F) < 0.000001F);

    decision.action = RECEIVER_FAILSAFE_ACTION_STOP;
    failsafe_result = MOTOR_CONTROL_FAILSAFE_ACCEPTED;
    assert(flight_control_process_receiver(&config, &decision, 43U) ==
           FLIGHT_CONTROL_FAILSAFE_ENTERED);
    assert(failsafe_count == 1U);
    assert(submit_count == 1U);

    failsafe_result = MOTOR_CONTROL_FAILSAFE_TRANSITION_ERROR;
    assert(flight_control_process_receiver(&config, &decision, 44U) ==
           FLIGHT_CONTROL_FAILSAFE_ERROR);
    assert(failsafe_count == 2U);

    decision = live_decision();
    decision.requested_control.valid = false;
    assert(flight_control_process_receiver(&config, &decision, 45U) ==
           FLIGHT_CONTROL_MIX_ERROR);

    {
        receiver_failsafe_t failsafe = {
            .stage_two_latched = true,
            .recovery_ready = true,
            .initialized = true,
        };

        assert(flight_control_recover_receiver(&failsafe) ==
               FLIGHT_CONTROL_RECOVERED);
        assert(recovery_count == 1U);
        assert(!failsafe.stage_two_latched);
        assert(!failsafe.recovery_ready);
        assert(flight_control_recover_receiver(&failsafe) ==
               FLIGHT_CONTROL_RECOVERY_ERROR);
        assert(recovery_count == 1U);
    }
    return 0;
}
