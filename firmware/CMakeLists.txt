set(STM32CUBE_F4_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/STM32CubeF4"
    CACHE PATH
    "Path to the pinned STM32CubeF4 package"
)

set(STM32_HAL_ROOT "${STM32CUBE_F4_ROOT}/Drivers/STM32F4xx_HAL_Driver")
set(STM32_CMSIS_ROOT "${STM32CUBE_F4_ROOT}/Drivers/CMSIS")
set(STM32_DEVICE_ROOT "${STM32_CMSIS_ROOT}/Device/ST/STM32F4xx")
set(STM32_USB_DEVICE_ROOT
    "${STM32CUBE_F4_ROOT}/Middlewares/ST/STM32_USB_Device_Library"
)
set(BMI270_SENSOR_API_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/BMI270_SensorAPI"
)

set(OPENFLIGHTCOMPUTER_USB_VID "0xCAFE" CACHE STRING
    "Development-only USB vendor ID; replace before distributing hardware"
)
set(OPENFLIGHTCOMPUTER_USB_PID "0x4002" CACHE STRING
    "Development-only flight-firmware USB product ID"
)
option(OPENFLIGHTCOMPUTER_USB_IDS_ARE_DEVELOPMENT
    "Confirm that the configured VID/PID are temporary development identifiers"
    ON
)

math(EXPR OPENFLIGHTCOMPUTER_USB_VID_VALUE "${OPENFLIGHTCOMPUTER_USB_VID}")
math(EXPR OPENFLIGHTCOMPUTER_USB_PID_VALUE "${OPENFLIGHTCOMPUTER_USB_PID}")
if(OPENFLIGHTCOMPUTER_USB_VID_VALUE LESS 1 OR
   OPENFLIGHTCOMPUTER_USB_VID_VALUE GREATER 65535 OR
   OPENFLIGHTCOMPUTER_USB_PID_VALUE LESS 1 OR
   OPENFLIGHTCOMPUTER_USB_PID_VALUE GREATER 65535)
    message(FATAL_ERROR "USB VID and PID must each be between 0x0001 and 0xFFFF.")
endif()

if(OPENFLIGHTCOMPUTER_USB_IDS_ARE_DEVELOPMENT)
    message(STATUS
        "USB identity: ${OPENFLIGHTCOMPUTER_USB_VID}:${OPENFLIGHTCOMPUTER_USB_PID} "
        "(temporary development values; not approved for distribution)"
    )
elseif(OPENFLIGHTCOMPUTER_USB_VID_VALUE EQUAL 51966 AND
       OPENFLIGHTCOMPUTER_USB_PID_VALUE EQUAL 16386)
    message(FATAL_ERROR
        "The default 0xCAFE:0x4002 identity is development-only. "
        "Configure assigned VID/PID values before setting "
        "OPENFLIGHTCOMPUTER_USB_IDS_ARE_DEVELOPMENT=OFF."
    )
endif()

set(STM32_REQUIRED_FILES
    "${STM32_HAL_ROOT}/Inc/stm32f4xx_hal.h"
    "${STM32_CMSIS_ROOT}/Core/Include/core_cm4.h"
    "${STM32_DEVICE_ROOT}/Include/stm32f405xx.h"
    "${STM32_DEVICE_ROOT}/Source/Templates/gcc/startup_stm32f405xx.s"
    "${STM32_USB_DEVICE_ROOT}/Core/Inc/usbd_core.h"
    "${STM32_USB_DEVICE_ROOT}/Class/CDC/Inc/usbd_cdc.h"
    "${BMI270_SENSOR_API_ROOT}/bmi2.c"
    "${BMI270_SENSOR_API_ROOT}/bmi270.c"
    "${BMI270_SENSOR_API_ROOT}/bmi270.h"
)

foreach(required_file IN LISTS STM32_REQUIRED_FILES)
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Missing STM32CubeF4 dependency file: ${required_file}\n"
            "Initialize the firmware submodules as documented in README.md."
        )
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_C_COMPILER}" -dumpfullversion
    OUTPUT_VARIABLE ARM_GCC_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)

set(REQUIRED_ARM_GCC_VERSION "15.3.1")
if(NOT ARM_GCC_VERSION STREQUAL REQUIRED_ARM_GCC_VERSION)
    message(FATAL_ERROR
        "Arm GCC ${REQUIRED_ARM_GCC_VERSION} is required; found ${ARM_GCC_VERSION}."
    )
endif()

execute_process(
    COMMAND "${CMAKE_C_COMPILER}" -print-file-name=libc.a
    OUTPUT_VARIABLE ARM_NEWLIB_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)

if(NOT EXISTS "${ARM_NEWLIB_PATH}")
    message(FATAL_ERROR
        "The selected Arm toolchain does not include newlib. "
        "Install the complete gcc-arm-embedded distribution."
    )
endif()

message(STATUS "Firmware version: ${PROJECT_VERSION}")
message(STATUS "ARM GCC version: ${ARM_GCC_VERSION}")
message(STATUS "STM32CubeF4 package: v1.28.3")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/usb_identity.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/usb_identity.h"
    @ONLY
)

set(FIRMWARE_IDENTITY_SOURCE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/firmware_identity.c"
)
add_custom_target(generate_firmware_identity ALL
    COMMAND "${CMAKE_COMMAND}"
        -DSOURCE_DIRECTORY=${PROJECT_SOURCE_DIR}
        -DFIRMWARE_VERSION=${PROJECT_VERSION}
        -DOUTPUT_FILE=${FIRMWARE_IDENTITY_SOURCE}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_firmware_identity.cmake"
    BYPRODUCTS "${FIRMWARE_IDENTITY_SOURCE}"
    COMMENT "Refreshing dirty-aware firmware build identity"
    VERBATIM
)
set_source_files_properties("${FIRMWARE_IDENTITY_SOURCE}" PROPERTIES GENERATED TRUE)

add_library(stm32cube_f4 STATIC
    "${STM32_DEVICE_ROOT}/Source/Templates/system_stm32f4xx.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_cortex.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_dma.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_flash.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_flash_ex.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_gpio.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_pcd.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_pcd_ex.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_pwr.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_pwr_ex.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_rcc.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_rcc_ex.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_spi.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_uart.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_ll_usb.c"
    "${STM32_USB_DEVICE_ROOT}/Core/Src/usbd_core.c"
    "${STM32_USB_DEVICE_ROOT}/Core/Src/usbd_ctlreq.c"
    "${STM32_USB_DEVICE_ROOT}/Core/Src/usbd_ioreq.c"
    "${STM32_USB_DEVICE_ROOT}/Class/CDC/Src/usbd_cdc.c"
)

target_include_directories(stm32cube_f4 SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/hardware/boards/flightcomputer_v1"
    "${CMAKE_CURRENT_SOURCE_DIR}/hardware/mcu/stm32f405"
    "${STM32_HAL_ROOT}/Inc"
    "${STM32_CMSIS_ROOT}/Core/Include"
    "${STM32_DEVICE_ROOT}/Include"
    "${STM32_USB_DEVICE_ROOT}/Core/Inc"
    "${STM32_USB_DEVICE_ROOT}/Class/CDC/Inc"
)

target_compile_definitions(stm32cube_f4 PUBLIC
    STM32F405xx
    USE_HAL_DRIVER
)

add_executable(flight_firmware
    app/application_runtime.c
    app/application_state.c
    app/application_tasks.c
    app/board_flight_configuration_storage.c
    app/diagnostic_fast_task.c
    app/diagnostic_medium_task.c
    app/diagnostic_slow_task.c
    app/dshot_motor_backend.c
    app/flight_configuration_service.c
    app/flight_control.c
    app/flight_control_task.c
    app/motor_control.c
    app/motor_safety_policy.c
    app/receiver_arming.c
    app/receiver_service.c
    app/fault.c
    app/fault_catalog.c
    app/health.c
    app/imu_service.c
    app/imu_task.c
    app/logging.c
    app/logging_config.c
    app/main.c
    app/motor_control_task.c
    app/receiver_task.c
    app/runtime_hooks.c
    app/scheduler.c
    app/system_state.c
    app/status_indicator.c
    app/status_indicator_task.c
    app/startup_task.c
    app/task.c
    app/usb_command_processor.c
    app/usb_health_response.c
    app/usb_imu_response.c
    app/usb_logging_backend.c
    app/usb_receiver_response.c
    app/usb_service_task.c
    "${FIRMWARE_IDENTITY_SOURCE}"
    flight/actuators/motor_command.c
    flight/actuators/motor_configuration.c
    flight/actuators/motor_mapping.c
    flight/actuators/motor_output.c
    flight/configuration/flight_configuration.c
    flight/control/quad_x_mixer.c
    flight/control/control_curve.c
    flight/control/control_input_shaping.c
    flight/control/rate_pid.c
    flight/control/rate_controller.c
    flight/estimation/accelerometer_attitude.c
    flight/estimation/attitude_estimator.c
    flight/estimation/imu_processing_pipeline.c
    flight/sensors/gyro_filter.c
    flight/sensors/imu_sample.c
    flight/sensors/gyro_calibration.c
    flight/receiver/receiver_freshness.c
    flight/receiver/receiver_failsafe.c
    flight/receiver/receiver_normalization.c
    flight/receiver/receiver_source.h
    peripherals/bmi270/bmi270_driver.c
    peripherals/dshot/dshot_encoder.c
    peripherals/dshot/dshot_timing.c
    peripherals/crsf/crsf_decoder.c
    peripherals/crsf/crsf_parser.c
    peripherals/crsf/crsf_receiver_source.c
    peripherals/usb/newline_framer.c
    peripherals/usb/usb_cdc_transport.c
    peripherals/usb/usb_descriptors.c
    peripherals/usb/usb_json_protocol.c
    hardware/boards/flightcomputer_v1/board.c
    hardware/boards/flightcomputer_v1/board_imu.c
    hardware/boards/flightcomputer_v1/board_motor_output.c
    hardware/boards/flightcomputer_v1/board_persistent_storage.c
    hardware/boards/flightcomputer_v1/board_receiver.c
    hardware/boards/flightcomputer_v1/motor_output_map.c
    hardware/boards/flightcomputer_v1/rgb_led.c
    hardware/boards/flightcomputer_v1/time.c
    hardware/boards/flightcomputer_v1/usb_device_port.c
    hardware/mcu/stm32f405/mcu.c
    hardware/mcu/stm32f405/mcu_timebase.c
    hardware/mcu/stm32f405/stm32f4xx_it.c
    hardware/mcu/stm32f405/system_clock.c
    hardware/mcu/stm32f405/timebase_snapshot.c
    hardware/spi_device.c
    "${BMI270_SENSOR_API_ROOT}/bmi2.c"
    "${BMI270_SENSOR_API_ROOT}/bmi270.c"
    "${STM32_DEVICE_ROOT}/Source/Templates/gcc/startup_stm32f405xx.s"
)
add_dependencies(flight_firmware generate_firmware_identity)

target_include_directories(flight_firmware PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/app"
    "${CMAKE_CURRENT_SOURCE_DIR}/flight/actuators"
    "${CMAKE_CURRENT_SOURCE_DIR}/flight/configuration"
    "${CMAKE_CURRENT_SOURCE_DIR}/flight/control"
    "${CMAKE_CURRENT_SOURCE_DIR}/flight/estimation"
    "${CMAKE_CURRENT_SOURCE_DIR}/flight/receiver"
    "${CMAKE_CURRENT_SOURCE_DIR}/flight/sensors"
    "${CMAKE_CURRENT_SOURCE_DIR}/hardware"
    "${CMAKE_CURRENT_SOURCE_DIR}/hardware/boards/flightcomputer_v1"
    "${CMAKE_CURRENT_SOURCE_DIR}/hardware/mcu/stm32f405"
    "${CMAKE_CURRENT_SOURCE_DIR}/peripherals/dshot"
    "${CMAKE_CURRENT_SOURCE_DIR}/peripherals/bmi270"
    "${CMAKE_CURRENT_SOURCE_DIR}/peripherals/crsf"
    "${CMAKE_CURRENT_SOURCE_DIR}/peripherals/usb"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    "${CMAKE_BINARY_DIR}/generated"
    "${BMI270_SENSOR_API_ROOT}"
)

target_link_libraries(flight_firmware PRIVATE firmware_common stm32cube_f4 m)

target_compile_options(flight_firmware PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Wall>
    $<$<COMPILE_LANGUAGE:C>:-Wextra>
    $<$<COMPILE_LANGUAGE:C>:-Wpedantic>
    $<$<COMPILE_LANGUAGE:C>:-Werror>
)

set(LINKER_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/hardware/mcu/stm32f405/STM32F405RGTX_FLASH.ld"
)
set(FIRMWARE_OUTPUT_NAME "openflightcomputer-flight-firmware")

target_link_options(flight_firmware PRIVATE
    "-T${LINKER_SCRIPT}"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_OUTPUT_NAME}.map"
    "-Wl,--gc-sections"
    "-Wl,--print-memory-usage"
    "--specs=nano.specs"
    "--specs=nosys.specs"
    "-nostartfiles"
)

set_target_properties(flight_firmware PROPERTIES
    OUTPUT_NAME "${FIRMWARE_OUTPUT_NAME}"
    SUFFIX ".elf"
    LINK_DEPENDS "${LINKER_SCRIPT}"
)

add_custom_command(TARGET flight_firmware POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex
        "$<TARGET_FILE:flight_firmware>"
        "${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_OUTPUT_NAME}.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary
        "$<TARGET_FILE:flight_firmware>"
        "${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_OUTPUT_NAME}.bin"
    COMMAND "${CMAKE_SIZE}" --format=berkeley "$<TARGET_FILE:flight_firmware>"
    COMMENT "Generating flashable HEX and BIN images"
)
