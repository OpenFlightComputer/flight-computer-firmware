add_library(openflightcomputer_host_runtime STATIC
    ../app/application_runtime_services.c
    ../app/application_state.c
    ../app/application_tasks.c
    ../app/blackbox.c
    ../app/control_trace.c
    ../app/diagnostic_fast_task.c
    ../app/diagnostic_medium_task.c
    ../app/diagnostic_slow_task.c
    ../app/fault.c
    ../app/fault_catalog.c
    ../app/flight_configuration_service.c
    ../app/flight_control.c
    ../app/flight_control_task.c
    ../app/flight_diagnostics.c
    ../app/health.c
    ../app/imu_service.c
    ../app/imu_task.c
    ../app/logging.c
    ../app/logging_config.c
    ../app/motor_control.c
    ../app/motor_control_task.c
    ../app/motor_safety_policy.c
    ../app/receiver_arming.c
    ../app/receiver_service.c
    ../app/receiver_task.c
    ../app/scheduler.c
    ../app/startup_task.c
    ../app/status_indicator.c
    ../app/system_state.c
    ../app/task.c
    ../flight/actuators/motor_command.c
    ../flight/actuators/motor_configuration.c
    ../flight/actuators/motor_mapping.c
    ../flight/actuators/motor_output.c
    ../flight/behavior/common/flight_behavior_configuration.c
    ../flight/behavior/manual_easy/manual_easy_behavior.c
    ../flight/configuration/flight_configuration.c
    ../flight/configuration/flight_runtime_configuration.c
    ../flight/control/control_curve.c
    ../flight/control/control_input_shaping.c
    ../flight/control/flight_control_core.c
    ../flight/control/pitch_attitude_controller.c
    ../flight/control/quad_x_mixer.c
    ../flight/control/rate_controller.c
    ../flight/control/rate_pid.c
    ../flight/control/roll_attitude_controller.c
    ../flight/control/takeoff_leveling.c
    ../flight/control/vehicle_state.c
    ../flight/control/yaw_attitude_controller.c
    ../flight/estimation/accelerometer_attitude.c
    ../flight/estimation/attitude_estimator.c
    ../flight/estimation/imu_processing_pipeline.c
    ../flight/receiver/receiver_failsafe.c
    ../flight/receiver/receiver_freshness.c
    ../flight/receiver/receiver_normalization.c
    ../flight/sensors/acceleration_filter.c
    ../flight/sensors/gyro_calibration.c
    ../flight/sensors/gyro_filter.c
    ../flight/sensors/imu_sample.c
    ../flight/sensors/level_calibration.c
    ../peripherals/sd_card/sd_card.c
    ../peripherals/sd_card/sd_card_csd.c
    ../hardware/spi_device.c
)
add_library(OpenFlightComputer::host_runtime ALIAS
    openflightcomputer_host_runtime
)

target_include_directories(openflightcomputer_host_runtime PUBLIC
    "${PROJECT_SOURCE_DIR}/firmware/app"
    "${PROJECT_SOURCE_DIR}/firmware/common"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators"
    "${PROJECT_SOURCE_DIR}/firmware/flight/behavior/common"
    "${PROJECT_SOURCE_DIR}/firmware/flight/behavior/manual_easy"
    "${PROJECT_SOURCE_DIR}/firmware/flight/configuration"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control"
    "${PROJECT_SOURCE_DIR}/firmware/flight/estimation"
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver"
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors"
    "${PROJECT_SOURCE_DIR}/firmware/hardware"
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/bmi270"
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/sd_card"
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/usb"
    "${CMAKE_BINARY_DIR}/generated"
)

target_link_libraries(openflightcomputer_host_runtime PUBLIC
    firmware_common
    m
)

target_compile_options(openflightcomputer_host_runtime PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
