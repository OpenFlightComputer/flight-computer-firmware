get_filename_component(OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
get_filename_component(OPENFLIGHTCOMPUTER_REPOSITORY_SOURCE_DIR
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/.." ABSOLUTE)

if(NOT TARGET firmware_common)
    add_subdirectory(
        "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/common"
        "${CMAKE_CURRENT_BINARY_DIR}/common"
    )
endif()

if(NOT DEFINED OPENFLIGHTCOMPUTER_GENERATED_INCLUDE_DIR)
    set(OPENFLIGHTCOMPUTER_GENERATED_INCLUDE_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/generated")
endif()
if(NOT OPENFLIGHTCOMPUTER_FLIGHT_CONFIGURATION_CONFIGURED)
    include(
        "${OPENFLIGHTCOMPUTER_REPOSITORY_SOURCE_DIR}/cmake/FlightConfiguration.cmake"
    )
endif()

add_library(openflightcomputer_host_runtime STATIC
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/application_runtime_services.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/application_state.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/application_tasks.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/blackbox.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/control_trace.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/diagnostic_fast_task.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/diagnostic_medium_task.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/diagnostic_slow_task.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/fault.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/fault_catalog.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/flight_configuration_service.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/flight_control.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/flight_control_task.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/flight_diagnostics.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/health.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/imu_service.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/imu_task.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/logging.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/logging_config.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/motor_control.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/motor_control_task.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/motor_safety_policy.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/receiver_arming.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/receiver_service.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/receiver_task.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/scheduler.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/startup_task.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/status_indicator.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/system_state.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app/task.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/actuators/motor_command.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/actuators/motor_configuration.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/actuators/motor_mapping.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/actuators/motor_output.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/behavior/common/flight_behavior_configuration.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/behavior/manual_easy/manual_easy_behavior.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/configuration/flight_configuration.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/configuration/flight_runtime_configuration.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/control/control_curve.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/control/control_input_shaping.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/control/flight_control_core.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/control/pitch_attitude_controller.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/control/quad_x_mixer.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/control/rate_controller.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/control/rate_pid.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/control/roll_attitude_controller.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/control/takeoff_leveling.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/control/vehicle_state.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/control/yaw_attitude_controller.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/estimation/accelerometer_attitude.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/estimation/attitude_estimator.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/estimation/imu_processing_pipeline.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/receiver/receiver_failsafe.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/receiver/receiver_freshness.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/receiver/receiver_normalization.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/sensors/acceleration_filter.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/sensors/gyro_calibration.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/sensors/gyro_filter.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/sensors/imu_sample.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/sensors/level_calibration.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/peripherals/sd_card/sd_card.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/peripherals/sd_card/sd_card_csd.c"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/hardware/spi_device.c"
)
add_library(OpenFlightComputer::host_runtime ALIAS
    openflightcomputer_host_runtime
)

target_include_directories(openflightcomputer_host_runtime PUBLIC
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/app"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/common"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/actuators"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/behavior/common"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/behavior/manual_easy"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/configuration"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/control"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/estimation"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/receiver"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/flight/sensors"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/hardware"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/peripherals/bmi270"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/peripherals/sd_card"
    "${OPENFLIGHTCOMPUTER_FIRMWARE_SOURCE_DIR}/peripherals/usb"
    "${OPENFLIGHTCOMPUTER_GENERATED_INCLUDE_DIR}"
)

target_link_libraries(openflightcomputer_host_runtime PUBLIC
    firmware_common
    m
)

target_compile_options(openflightcomputer_host_runtime PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
