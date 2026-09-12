add_executable(timebase_snapshot_tests
    timebase_snapshot_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/hardware/mcu/stm32f405/timebase_snapshot.c"
)

target_include_directories(timebase_snapshot_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/hardware/mcu/stm32f405"
)

target_compile_options(timebase_snapshot_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME timebase_snapshot COMMAND timebase_snapshot_tests)

add_executable(spi_device_tests
    spi_device_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/hardware/spi_device.c"
)

target_include_directories(spi_device_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/hardware"
)

target_compile_options(spi_device_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME spi_device COMMAND spi_device_tests)

add_executable(bmi270_driver_tests
    bmi270_driver_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/bmi270/bmi270_driver.c"
    "${PROJECT_SOURCE_DIR}/firmware/hardware/spi_device.c"
)

target_include_directories(bmi270_driver_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/hardware"
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/bmi270"
    "${PROJECT_SOURCE_DIR}/firmware/third_party/BMI270_SensorAPI"
)

target_compile_options(bmi270_driver_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME bmi270_driver COMMAND bmi270_driver_tests)

add_executable(imu_sample_tests
    imu_sample_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors/imu_sample.c"
)

target_include_directories(imu_sample_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors"
)

target_compile_options(imu_sample_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME imu_sample COMMAND imu_sample_tests)

add_executable(gyro_calibration_tests
    gyro_calibration_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors/gyro_calibration.c"
)
target_include_directories(gyro_calibration_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors"
)
target_link_libraries(gyro_calibration_tests PRIVATE m)
target_compile_options(gyro_calibration_tests PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
add_test(NAME gyro_calibration COMMAND gyro_calibration_tests)

add_executable(imu_processing_pipeline_tests
    imu_processing_pipeline_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/flight/estimation/accelerometer_attitude.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/estimation/attitude_estimator.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/estimation/imu_processing_pipeline.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors/gyro_filter.c"
)
target_include_directories(imu_processing_pipeline_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/flight/estimation"
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors"
)
target_link_libraries(imu_processing_pipeline_tests PRIVATE m)
target_compile_options(imu_processing_pipeline_tests PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
add_test(NAME imu_processing_pipeline COMMAND imu_processing_pipeline_tests)

add_executable(imu_service_tests
    imu_service_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/imu_service.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors/imu_sample.c"
)

target_include_directories(imu_service_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors"
)

target_compile_options(imu_service_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME imu_service COMMAND imu_service_tests)

add_executable(task_tests
    task_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/task.c"
)

target_include_directories(task_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
)

target_compile_options(task_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME task COMMAND task_tests)

add_executable(control_trace_tests
    control_trace_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/control_trace.c"
)
target_include_directories(control_trace_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators"
    "${PROJECT_SOURCE_DIR}/firmware/flight/configuration"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control"
    "${PROJECT_SOURCE_DIR}/firmware/flight/estimation"
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver"
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors"
)
target_compile_options(control_trace_tests PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
add_test(NAME control_trace COMMAND control_trace_tests)

add_executable(receiver_service_tests
    receiver_service_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/receiver_service.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver/receiver_freshness.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver/receiver_normalization.c"
)

target_include_directories(receiver_service_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver"
)

target_compile_options(receiver_service_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME receiver_service COMMAND receiver_service_tests)

add_executable(receiver_normalization_tests
    receiver_normalization_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver/receiver_normalization.c"
)

target_include_directories(receiver_normalization_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver"
)

target_compile_options(receiver_normalization_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME receiver_normalization COMMAND receiver_normalization_tests)

add_executable(receiver_freshness_tests
    receiver_freshness_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver/receiver_freshness.c"
)

target_include_directories(receiver_freshness_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver"
)

target_compile_options(receiver_freshness_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME receiver_freshness COMMAND receiver_freshness_tests)

add_executable(receiver_failsafe_tests
    receiver_failsafe_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver/receiver_failsafe.c"
)

target_include_directories(receiver_failsafe_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver"
)

target_compile_options(receiver_failsafe_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME receiver_failsafe COMMAND receiver_failsafe_tests)

add_executable(receiver_arming_tests
    receiver_arming_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/receiver_arming.c"
)

target_include_directories(receiver_arming_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators"
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver"
)

target_compile_options(receiver_arming_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME receiver_arming COMMAND receiver_arming_tests)

add_executable(crsf_tests
    crsf_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/crsf/crsf_decoder.c"
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/crsf/crsf_parser.c"
)

target_include_directories(crsf_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/crsf"
)

target_compile_options(crsf_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME crsf COMMAND crsf_tests)

add_executable(crsf_receiver_source_tests
    crsf_receiver_source_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/crsf/crsf_decoder.c"
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/crsf/crsf_parser.c"
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/crsf/crsf_receiver_source.c"
)

target_include_directories(crsf_receiver_source_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver"
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/crsf"
)

target_compile_options(crsf_receiver_source_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME crsf_receiver_source COMMAND crsf_receiver_source_tests)

add_executable(scheduler_tests
    scheduler_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/scheduler.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/task.c"
)

target_include_directories(scheduler_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
)

target_compile_options(scheduler_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME scheduler COMMAND scheduler_tests)

add_executable(system_state_tests
    system_state_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/system_state.c"
)

target_include_directories(system_state_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
)

target_compile_options(system_state_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME system_state COMMAND system_state_tests)

add_executable(fault_tests
    fault_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/fault.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/fault_catalog.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/system_state.c"
)

target_include_directories(fault_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
)

target_compile_options(fault_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME fault COMMAND fault_tests)

add_executable(health_tests
    health_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/fault.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/health.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/system_state.c"
)

target_include_directories(health_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
)

target_compile_options(health_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME health COMMAND health_tests)

add_executable(foundation_integration_tests
    foundation_integration_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/fault.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/health.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/system_state.c"
)

target_include_directories(foundation_integration_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
)

target_compile_options(foundation_integration_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME foundation_integration COMMAND foundation_integration_tests)

add_executable(motor_safety_policy_tests
    motor_safety_policy_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/motor_safety_policy.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/fault.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/health.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/system_state.c"
)

target_include_directories(motor_safety_policy_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
)

target_compile_options(motor_safety_policy_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME motor_safety_policy COMMAND motor_safety_policy_tests)

add_executable(motor_control_tests
    motor_control_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/motor_control.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/motor_safety_policy.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/fault.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/fault_catalog.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/health.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/system_state.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_command.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_configuration.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_mapping.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_output.c"
)

target_include_directories(motor_control_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators"
)

target_compile_options(motor_control_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME motor_control COMMAND motor_control_tests)

add_test(
    NAME motor_architecture
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_motor_boundaries.cmake"
)

add_executable(motor_command_tests
    motor_command_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_command.c"
)

target_include_directories(motor_command_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators"
)

target_compile_options(motor_command_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME motor_command COMMAND motor_command_tests)

add_executable(motor_configuration_tests
    motor_configuration_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_configuration.c"
)

target_include_directories(motor_configuration_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators"
)

target_compile_options(motor_configuration_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME motor_configuration COMMAND motor_configuration_tests)

add_executable(board_flight_configuration_storage_tests
    board_flight_configuration_storage_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/board_flight_configuration_storage.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/configuration/flight_configuration.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/quad_x_mixer.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/control_curve.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/control_input_shaping.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/roll_attitude_controller.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/pitch_attitude_controller.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/rate_pid.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/rate_controller.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver/receiver_failsafe.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_command.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_configuration.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/estimation/attitude_estimator.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors/gyro_filter.c"
)

target_include_directories(board_flight_configuration_storage_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators"
    "${PROJECT_SOURCE_DIR}/firmware/flight/configuration"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control"
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver"
    "${PROJECT_SOURCE_DIR}/firmware/flight/estimation"
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors"
    "${PROJECT_SOURCE_DIR}/firmware/hardware/boards/flightcomputer_v1"
    "${CMAKE_BINARY_DIR}/generated"
)

target_compile_options(board_flight_configuration_storage_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME board_flight_configuration_storage
         COMMAND board_flight_configuration_storage_tests)

add_executable(flight_configuration_service_tests
    flight_configuration_service_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/flight_configuration_service.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/configuration/flight_configuration.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/quad_x_mixer.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/control_curve.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/control_input_shaping.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/roll_attitude_controller.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/pitch_attitude_controller.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/rate_pid.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/rate_controller.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_command.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_configuration.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/estimation/accelerometer_attitude.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/estimation/attitude_estimator.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/estimation/imu_processing_pipeline.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors/gyro_filter.c"
)
target_include_directories(flight_configuration_service_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators"
    "${PROJECT_SOURCE_DIR}/firmware/flight/configuration"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control"
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver"
    "${PROJECT_SOURCE_DIR}/firmware/flight/estimation"
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors"
    "${CMAKE_BINARY_DIR}/generated"
)
target_compile_options(flight_configuration_service_tests PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
target_link_libraries(flight_configuration_service_tests PRIVATE m)
add_test(NAME flight_configuration_service
         COMMAND flight_configuration_service_tests)

add_executable(quad_x_mixer_tests
    quad_x_mixer_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/quad_x_mixer.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_command.c"
)
target_include_directories(quad_x_mixer_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control"
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver"
)
target_compile_options(quad_x_mixer_tests PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
add_test(NAME quad_x_mixer COMMAND quad_x_mixer_tests)

add_executable(control_input_shaping_tests
    control_input_shaping_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/control_curve.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/control_input_shaping.c"
)
target_include_directories(control_input_shaping_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/flight/control"
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver"
)
target_link_libraries(control_input_shaping_tests PRIVATE m)
target_compile_options(control_input_shaping_tests PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
add_test(NAME control_input_shaping COMMAND control_input_shaping_tests)

add_executable(rate_pid_tests
    rate_pid_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/rate_pid.c"
)
target_include_directories(rate_pid_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/flight/control"
)
target_link_libraries(rate_pid_tests PRIVATE m)
target_compile_options(rate_pid_tests PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
add_test(NAME rate_pid COMMAND rate_pid_tests)

add_executable(rate_controller_tests
    rate_controller_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/rate_pid.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/rate_controller.c"
)
target_include_directories(rate_controller_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/flight/control"
)
target_link_libraries(rate_controller_tests PRIVATE m)
target_compile_options(rate_controller_tests PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
add_test(NAME rate_controller COMMAND rate_controller_tests)

add_executable(attitude_controller_tests
    attitude_controller_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/roll_attitude_controller.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/pitch_attitude_controller.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/yaw_attitude_controller.c"
)
target_include_directories(attitude_controller_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/flight/control"
)
target_link_libraries(attitude_controller_tests PRIVATE m)
target_compile_options(attitude_controller_tests PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
add_test(NAME attitude_controller COMMAND attitude_controller_tests)

add_executable(flight_control_tests
    flight_control_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/flight_control.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/quad_x_mixer.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/control_curve.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/control_input_shaping.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/roll_attitude_controller.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/pitch_attitude_controller.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/yaw_attitude_controller.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/rate_pid.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control/rate_controller.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_command.c"
)
target_include_directories(flight_control_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators"
    "${PROJECT_SOURCE_DIR}/firmware/flight/configuration"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control"
    "${PROJECT_SOURCE_DIR}/firmware/flight/estimation"
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors"
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver"
)
target_compile_options(flight_control_tests PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
add_test(NAME flight_control COMMAND flight_control_tests)

add_executable(motor_mapping_tests
    motor_mapping_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_command.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_mapping.c"
)

target_include_directories(motor_mapping_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators"
)

target_compile_options(motor_mapping_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME motor_mapping COMMAND motor_mapping_tests)

add_executable(motor_output_tests
    motor_output_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_command.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_configuration.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_output.c"
)

target_include_directories(motor_output_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators"
)

target_compile_options(motor_output_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME motor_output COMMAND motor_output_tests)

add_executable(dshot_motor_backend_tests
    dshot_motor_backend_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/dshot_motor_backend.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_command.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_configuration.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_output.c"
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/dshot/dshot_encoder.c"
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/dshot/dshot_timing.c"
)

target_include_directories(dshot_motor_backend_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators"
    "${PROJECT_SOURCE_DIR}/firmware/hardware/boards/flightcomputer_v1"
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/dshot"
)

target_compile_options(dshot_motor_backend_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME dshot_motor_backend COMMAND dshot_motor_backend_tests)

add_executable(dshot_encoder_tests
    dshot_encoder_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/dshot/dshot_encoder.c"
)

target_include_directories(dshot_encoder_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/dshot"
)

target_compile_options(dshot_encoder_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME dshot_encoder COMMAND dshot_encoder_tests)

add_executable(dshot_timing_tests
    dshot_timing_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/dshot/dshot_timing.c"
)

target_include_directories(dshot_timing_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/dshot"
)

target_compile_options(dshot_timing_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME dshot_timing COMMAND dshot_timing_tests)

add_executable(board_motor_output_map_tests
    board_motor_output_map_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/hardware/boards/flightcomputer_v1/motor_output_map.c"
)

target_include_directories(board_motor_output_map_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/hardware"
    "${PROJECT_SOURCE_DIR}/firmware/hardware/boards/flightcomputer_v1"
)

target_compile_options(board_motor_output_map_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME board_motor_output_map COMMAND board_motor_output_map_tests)

add_executable(board_usb_tests
    board_usb_tests.c
)

target_include_directories(board_usb_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/hardware"
    "${PROJECT_SOURCE_DIR}/firmware/hardware/boards/flightcomputer_v1"
)

target_compile_options(board_usb_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME board_usb COMMAND board_usb_tests)

add_executable(uint64_decimal_tests
    uint64_decimal_tests.c
)

target_link_libraries(uint64_decimal_tests PRIVATE firmware_common)

target_compile_options(uint64_decimal_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME uint64_decimal COMMAND uint64_decimal_tests)

add_executable(logging_tests
    logging_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/logging.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/logging_config.c"
)

target_include_directories(logging_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
)

target_link_libraries(logging_tests PRIVATE firmware_common)

target_compile_options(logging_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME logging COMMAND logging_tests)

add_executable(usb_logging_backend_tests
    usb_logging_backend_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/logging.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/logging_config.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/usb_logging_backend.c"
)

target_include_directories(usb_logging_backend_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/usb"
)

target_link_libraries(usb_logging_backend_tests PRIVATE firmware_common)

target_compile_options(usb_logging_backend_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME usb_logging_backend COMMAND usb_logging_backend_tests)

add_executable(newline_framer_tests
    newline_framer_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/usb/newline_framer.c"
)

target_include_directories(newline_framer_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/usb"
)

target_compile_options(newline_framer_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME newline_framer COMMAND newline_framer_tests)

add_executable(usb_json_protocol_tests
    usb_json_protocol_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/usb/usb_json_protocol.c"
)

target_include_directories(usb_json_protocol_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/usb"
)

target_link_libraries(usb_json_protocol_tests PRIVATE firmware_common)

target_compile_options(usb_json_protocol_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME usb_json_protocol COMMAND usb_json_protocol_tests)

add_executable(usb_health_response_tests
    usb_health_response_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/fault.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/health.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/system_state.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/usb_health_response.c"
)

target_include_directories(usb_health_response_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
)

target_link_libraries(usb_health_response_tests PRIVATE firmware_common)

target_compile_options(usb_health_response_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME usb_health_response COMMAND usb_health_response_tests)

add_executable(usb_receiver_response_tests
    usb_receiver_response_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/usb_receiver_response.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver/receiver_failsafe.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver/receiver_freshness.c"
)

target_include_directories(usb_receiver_response_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver"
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/usb"
)

target_link_libraries(usb_receiver_response_tests PRIVATE firmware_common)

target_compile_options(usb_receiver_response_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME usb_receiver_response COMMAND usb_receiver_response_tests)

add_executable(usb_imu_response_tests
    usb_imu_response_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/usb_imu_response.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors/imu_sample.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors/gyro_calibration.c"
)

target_include_directories(usb_imu_response_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
    "${PROJECT_SOURCE_DIR}/firmware/flight/estimation"
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors"
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/usb"
)

target_link_libraries(usb_imu_response_tests PRIVATE firmware_common)

target_compile_options(usb_imu_response_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME usb_imu_response COMMAND usb_imu_response_tests)

add_executable(usb_command_processor_tests
    usb_command_processor_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/fault.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/health.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/motor_safety_policy.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/logging.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/logging_config.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/system_state.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/usb_command_processor.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/control_trace.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/usb_control_trace_response.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/usb_health_response.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/usb_imu_response.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/usb_receiver_response.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/imu_service.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/task.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors/imu_sample.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors/gyro_calibration.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver/receiver_failsafe.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver/receiver_freshness.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_command.c"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators/motor_configuration.c"
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/usb/usb_json_protocol.c"
)

target_include_directories(usb_command_processor_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
    "${PROJECT_SOURCE_DIR}/firmware/flight/actuators"
    "${PROJECT_SOURCE_DIR}/firmware/flight/configuration"
    "${PROJECT_SOURCE_DIR}/firmware/flight/control"
    "${PROJECT_SOURCE_DIR}/firmware/flight/estimation"
    "${PROJECT_SOURCE_DIR}/firmware/flight/receiver"
    "${PROJECT_SOURCE_DIR}/firmware/flight/sensors"
    "${PROJECT_SOURCE_DIR}/firmware/hardware/boards/flightcomputer_v1"
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/usb"
)

target_link_libraries(usb_command_processor_tests PRIVATE firmware_common)

target_compile_options(usb_command_processor_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME usb_command_processor COMMAND usb_command_processor_tests)
