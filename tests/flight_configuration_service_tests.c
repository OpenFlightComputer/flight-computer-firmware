#include "flight_configuration_service.h"

#include "motor_control.h"

#include <assert.h>
#include <string.h>

typedef struct {
    flight_configuration_load_result_t load_result;
    flight_configuration_save_result_t save_result;
    flight_configuration_clear_result_t clear_result;
    flight_configuration_t stored;
    uint32_t load_count;
    uint32_t save_count;
    uint32_t clear_count;
} fake_storage_t;

static motor_control_source_t pending_source;
static motor_control_configuration_apply_result_t motor_apply_result;
static motor_configuration_t applied_motors;
static receiver_freshness_config_t applied_freshness;
static receiver_failsafe_config_t applied_failsafe;
static uint32_t motor_apply_count;
static uint32_t freshness_apply_count;
static uint32_t failsafe_apply_count;
static uint64_t now_us;

motor_control_source_t motor_control_pending_source(void)
{
    return pending_source;
}

motor_control_configuration_apply_result_t motor_control_apply_configuration(
    const motor_configuration_t *configuration)
{
    motor_apply_count++;
    applied_motors = *configuration;
    return motor_apply_result;
}

bool receiver_service_update_freshness_config(
    receiver_service_t *service,
    const receiver_freshness_config_t *config)
{
    assert(service != NULL);
    freshness_apply_count++;
    applied_freshness = *config;
    return true;
}

bool receiver_failsafe_initialize(receiver_failsafe_t *failsafe,
                                  const receiver_failsafe_config_t *config,
                                  uint64_t initialized_at_us)
{
    assert(failsafe != NULL);
    failsafe_apply_count++;
    applied_failsafe = *config;
    failsafe->initialized = true;
    failsafe->initialized_at_us = initialized_at_us;
    return true;
}

bool receiver_failsafe_config_is_valid(
    const receiver_failsafe_config_t *config)
{
    return (config != NULL) &&
           (config->stale_after_us <= config->loss_detected_after_us) &&
           (config->loss_detected_after_us <= config->hold_last_until_us) &&
           (config->hold_last_until_us <= config->stage_two_after_us);
}

static uint64_t fake_clock(void)
{
    return now_us;
}

static flight_configuration_load_result_t fake_load(
    void *context,
    flight_configuration_t *configuration)
{
    fake_storage_t *storage = context;

    storage->load_count++;
    if (storage->load_result == FLIGHT_CONFIGURATION_LOAD_OK) {
        *configuration = storage->stored;
    }
    return storage->load_result;
}

static flight_configuration_save_result_t fake_save(
    void *context,
    const flight_configuration_t *configuration)
{
    fake_storage_t *storage = context;

    storage->save_count++;
    if (storage->save_result == FLIGHT_CONFIGURATION_SAVE_OK) {
        storage->stored = *configuration;
    }
    return storage->save_result;
}

static flight_configuration_clear_result_t fake_clear(void *context)
{
    fake_storage_t *storage = context;

    storage->clear_count++;
    return storage->clear_result;
}

static flight_configuration_storage_t storage_interface(
    fake_storage_t *storage)
{
    return (flight_configuration_storage_t){
        .load = fake_load,
        .save = fake_save,
        .clear = fake_clear,
        .context = storage,
    };
}

int main(void)
{
    fake_storage_t storage = {
        .load_result = FLIGHT_CONFIGURATION_LOAD_EMPTY,
        .save_result = FLIGHT_CONFIGURATION_SAVE_OK,
        .clear_result = FLIGHT_CONFIGURATION_CLEAR_OK,
    };
    const flight_configuration_storage_t interface =
        storage_interface(&storage);
    system_state_machine_t state_machine = {
        .current = SYSTEM_STATE_DISARMED,
        .initialized = true,
    };
    receiver_failsafe_t failsafe = {0};
    receiver_service_t receiver_service = {.initialized = true};
    flight_configuration_service_t service;
    flight_configuration_t configuration;
    flight_configuration_t read_back;
    flight_configuration_source_t source;

    assert(flight_configuration_service_initialize(
               &service,
               &interface,
               &state_machine,
               &failsafe,
               &receiver_service,
               fake_clock) == FLIGHT_CONFIGURATION_SERVICE_OK);
    assert(storage.load_count == 1U);
    assert(service.source == FLIGHT_CONFIGURATION_SOURCE_DEFAULT);
    assert(service.active.propeller_layout == PROPELLER_LAYOUT_PROPS_IN);

    configuration = service.active;
    configuration.propeller_layout = PROPELLER_LAYOUT_PROPS_OUT;
    configuration.motors.direction[2] = MOTOR_DIRECTION_REVERSED;
    configuration.mixer.yaw_factor = 0.2F;
    configuration.receiver_failsafe.stale_after_us = 30000U;
    now_us = 123U;
    assert(flight_configuration_service_write(&service, &configuration) ==
           FLIGHT_CONFIGURATION_SERVICE_OK);
    assert(storage.save_count == 1U);
    assert(motor_apply_count == 1U);
    assert(freshness_apply_count == 1U);
    assert(failsafe_apply_count == 1U);
    assert(applied_motors.direction[2] == MOTOR_DIRECTION_REVERSED);
    assert(applied_freshness.fresh_through_us == 30000U);
    assert(applied_failsafe.stale_after_us == 30000U);
    assert(failsafe.initialized_at_us == 123U);
    assert(service.source == FLIGHT_CONFIGURATION_SOURCE_PERSISTENT);

    state_machine.current = SYSTEM_STATE_ARMED;
    assert(flight_configuration_service_write(&service, &configuration) ==
           FLIGHT_CONFIGURATION_SERVICE_UNSAFE_STATE);
    assert(storage.save_count == 1U);
    state_machine.current = SYSTEM_STATE_DISARMED;
    pending_source = MOTOR_CONTROL_SOURCE_RECEIVER;
    assert(flight_configuration_service_reset(&service) ==
           FLIGHT_CONFIGURATION_SERVICE_UNSAFE_STATE);
    assert(storage.clear_count == 0U);
    pending_source = MOTOR_CONTROL_SOURCE_NONE;

    assert(flight_configuration_service_reset(&service) ==
           FLIGHT_CONFIGURATION_SERVICE_OK);
    assert(storage.clear_count == 1U);
    assert(service.source == FLIGHT_CONFIGURATION_SOURCE_DEFAULT);
    assert(service.active.propeller_layout == PROPELLER_LAYOUT_PROPS_IN);
    assert(service.active.motors.direction[2] == MOTOR_DIRECTION_NORMAL);

    assert(flight_configuration_service_read(
        &service, &read_back, &source));
    assert(source == FLIGHT_CONFIGURATION_SOURCE_DEFAULT);
    assert(memcmp(&read_back, &service.active, sizeof(read_back)) == 0);
    assert(strcmp(flight_configuration_source_name(source), "DEFAULT") == 0);
    assert(strcmp(flight_configuration_source_name(
                      FLIGHT_CONFIGURATION_SOURCE_PERSISTENT),
                  "PERSISTENT") == 0);

    storage.save_result = FLIGHT_CONFIGURATION_SAVE_ERROR;
    configuration = service.active;
    configuration.mixer.roll_factor = 0.3F;
    assert(flight_configuration_service_write(&service, &configuration) ==
           FLIGHT_CONFIGURATION_SERVICE_STORAGE_ERROR);
    assert(service.active.mixer.roll_factor == 0.25F);

    storage.load_result = FLIGHT_CONFIGURATION_LOAD_ERROR;
    assert(flight_configuration_service_initialize(
               &service,
               &interface,
               &state_machine,
               &failsafe,
               &receiver_service,
               fake_clock) == FLIGHT_CONFIGURATION_SERVICE_STORAGE_ERROR);
    return 0;
}
