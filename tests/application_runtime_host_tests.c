#include "application_runtime.h"

#include "application_state.h"
#include "flight_configuration.h"
#include "gyro_calibration.h"
#include "motor_command.h"
#include "system_state.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define STATIONARY_SENSOR_Z_COUNTS INT16_C(4096)
#define STARTUP_TICKS UINT32_C(600)
#define DRAIN_EXECUTION_LIMIT 32U

typedef struct {
    uint64_t now_us;
    uint32_t imu_reads;
    uint32_t receiver_reads;
    uint32_t motor_submissions;
    uint32_t motor_stops;
    bool powered_motor_command_seen;
    flight_configuration_t configuration;
} virtual_firmware_t;

static virtual_firmware_t virtual_firmware;

static uint64_t virtual_clock(void)
{
    return virtual_firmware.now_us;
}

static imu_source_result_t read_stationary_imu(
    void *context,
    imu_raw_sample_t *sample)
{
    virtual_firmware_t *simulation = context;

    assert(simulation != NULL);
    assert(sample != NULL);
    simulation->imu_reads++;
    *sample = (imu_raw_sample_t){
        .acceleration_z = STATIONARY_SENSOR_Z_COUNTS,
    };
    return IMU_SOURCE_SAMPLE_AVAILABLE;
}

static receiver_source_result_t read_disarmed_receiver(
    void *context,
    receiver_channel_frame_t *frame)
{
    virtual_firmware_t *simulation = context;

    assert(simulation != NULL);
    assert(frame != NULL);
    simulation->receiver_reads++;
    *frame = (receiver_channel_frame_t){0};
    frame->channels[0] = 992U;
    frame->channels[1] = 992U;
    frame->channels[2] = 174U;
    frame->channels[3] = 997U;
    frame->channels[4] = 174U;
    return RECEIVER_SOURCE_FRAME_AVAILABLE;
}

static flight_configuration_load_result_t load_configuration(
    void *context,
    flight_configuration_t *configuration)
{
    const virtual_firmware_t *simulation = context;

    assert(simulation != NULL);
    assert(configuration != NULL);
    *configuration = simulation->configuration;
    return FLIGHT_CONFIGURATION_LOAD_OK;
}

static flight_configuration_save_result_t save_configuration(
    void *context,
    const flight_configuration_t *configuration)
{
    virtual_firmware_t *simulation = context;

    assert(simulation != NULL);
    assert(configuration != NULL);
    simulation->configuration = *configuration;
    return FLIGHT_CONFIGURATION_SAVE_OK;
}

static flight_configuration_clear_result_t clear_configuration(void *context)
{
    virtual_firmware_t *simulation = context;

    assert(simulation != NULL);
    flight_configuration_defaults(&simulation->configuration);
    return FLIGHT_CONFIGURATION_CLEAR_OK;
}

static motor_output_backend_init_result_t initialize_motors(void *context)
{
    assert(context == &virtual_firmware);
    return MOTOR_OUTPUT_BACKEND_INIT_OK;
}

static motor_output_backend_submit_result_t submit_motors(
    const motor_command_t *command,
    void *context)
{
    virtual_firmware_t *simulation = context;
    size_t motor;

    assert(simulation != NULL);
    assert(command != NULL);
    simulation->motor_submissions++;
    for (motor = 0U; motor < MOTOR_COMMAND_MOTOR_COUNT; motor++) {
        if (command->throttle[motor] != 0.0F) {
            simulation->powered_motor_command_seen = true;
        }
    }
    return MOTOR_OUTPUT_BACKEND_SUBMIT_ACCEPTED;
}

static motor_output_backend_direction_result_t submit_directions(
    const motor_direction_t directions[MOTOR_COMMAND_MOTOR_COUNT],
    void *context)
{
    assert(directions != NULL);
    assert(context == &virtual_firmware);
    return MOTOR_OUTPUT_BACKEND_DIRECTION_ACCEPTED;
}

static motor_output_backend_stop_result_t stop_motors(void *context)
{
    virtual_firmware_t *simulation = context;

    assert(simulation != NULL);
    simulation->motor_stops++;
    return MOTOR_OUTPUT_BACKEND_STOP_ACCEPTED;
}

static motor_output_backend_status_t motor_status(void *context)
{
    assert(context == &virtual_firmware);
    return MOTOR_OUTPUT_BACKEND_STATUS_IDLE;
}

static uint32_t motor_diagnostic_context(void *context)
{
    assert(context == &virtual_firmware);
    return 0U;
}

static void drain_due_tasks(size_t expected_minimum)
{
    size_t executions = 0U;

    assert(application_runtime_run_until_idle(DRAIN_EXECUTION_LIMIT,
                                              &executions) ==
           APPLICATION_RUNTIME_DRAIN_IDLE);
    assert(executions >= expected_minimum);
}

int main(void)
{
    uint32_t tick;
    size_t bounded_executions = 0U;
    application_runtime_init_result_t initialization_result;
    const application_runtime_services_t services = {
        .clock = virtual_clock,
        .imu_source = {
            .read = read_stationary_imu,
            .context = &virtual_firmware,
        },
        .receiver_source = {
            .read = read_disarmed_receiver,
            .context = &virtual_firmware,
        },
        .configuration_storage = {
            .load = load_configuration,
            .save = save_configuration,
            .clear = clear_configuration,
            .context = &virtual_firmware,
        },
        .motor_output_backend = {
            .initialize = initialize_motors,
            .submit = submit_motors,
            .submit_directions = submit_directions,
            .force_stop = stop_motors,
            .status = motor_status,
            .diagnostic_context = motor_diagnostic_context,
            .context = &virtual_firmware,
        },
    };

    flight_configuration_defaults(&virtual_firmware.configuration);
    virtual_firmware.configuration.level_calibration.calibrated = true;
    assert(flight_configuration_is_valid(&virtual_firmware.configuration));
    initialization_result =
        application_runtime_initialize_with_services(&services);
    if (initialization_result != APPLICATION_RUNTIME_INIT_OK) {
        fprintf(stderr, "runtime initialization failed: %u motor=%u\n",
                (unsigned int)initialization_result,
                (unsigned int)firmware_motor_control_initialization_result);
        return 1;
    }
    assert(firmware_system_state_machine.current ==
           SYSTEM_STATE_INITIALIZING);

    assert(application_runtime_run_until_idle(1U, &bounded_executions) ==
           APPLICATION_RUNTIME_DRAIN_LIMIT_REACHED);
    assert(bounded_executions == 1U);
    drain_due_tasks(7U);
    for (tick = 1U; tick <= STARTUP_TICKS; tick++) {
        virtual_firmware.now_us = (uint64_t)tick * UINT64_C(1000);
        drain_due_tasks(5U);
    }

    assert(virtual_firmware.imu_reads == STARTUP_TICKS + 1U);
    assert(virtual_firmware.receiver_reads == STARTUP_TICKS + 1U);
    assert(firmware_imu_service.statistics.published_sample_count ==
           STARTUP_TICKS + 1U);
    assert(firmware_imu_task_executions == STARTUP_TICKS + 1U);
    assert(firmware_motor_control_task_executions == STARTUP_TICKS + 1U);
    assert(firmware_receiver_service.statistics.accepted_frame_count ==
           STARTUP_TICKS + 1U);
    assert(firmware_imu_body_acceleration_x == 0);
    assert(firmware_imu_body_acceleration_y == 0);
    assert(firmware_imu_body_acceleration_z == -4096);
    assert(firmware_gyro_calibration.state == GYRO_CALIBRATION_READY);
    assert(firmware_gyro_calibration.sample_count == 501U);
    assert(firmware_system_state_machine.current == SYSTEM_STATE_DISARMED);
    assert(firmware_boot_status == BOOT_STATUS_RUNNING);
    assert(!virtual_firmware.powered_motor_command_seen);
    assert(virtual_firmware.motor_stops == 1U);
    return 0;
}
