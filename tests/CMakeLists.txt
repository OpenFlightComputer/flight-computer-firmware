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

add_executable(usb_command_processor_tests
    usb_command_processor_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/fault.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/health.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/motor_safety_policy.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/logging.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/logging_config.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/system_state.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/usb_command_processor.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/usb_health_response.c"
    "${PROJECT_SOURCE_DIR}/firmware/peripherals/usb/usb_json_protocol.c"
)

target_include_directories(usb_command_processor_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
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
