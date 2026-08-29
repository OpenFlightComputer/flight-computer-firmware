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

add_library(stm32cube_f4 STATIC
    "${STM32_DEVICE_ROOT}/Source/Templates/system_stm32f4xx.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_cortex.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_flash.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_flash_ex.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_gpio.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_pcd.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_pcd_ex.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_pwr.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_pwr_ex.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_rcc.c"
    "${STM32_HAL_ROOT}/Src/stm32f4xx_hal_rcc_ex.c"
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
    app/fault.c
    app/fault_catalog.c
    app/logging.c
    app/logging_config.c
    app/main.c
    app/runtime_hooks.c
    app/scheduler.c
    app/system_state.c
    app/task.c
    app/usb_command_processor.c
    app/usb_logging_backend.c
    peripherals/usb/newline_framer.c
    peripherals/usb/usb_cdc_transport.c
    peripherals/usb/usb_descriptors.c
    peripherals/usb/usb_json_protocol.c
    hardware/boards/flightcomputer_v1/board.c
    hardware/boards/flightcomputer_v1/time.c
    hardware/boards/flightcomputer_v1/usb_device_port.c
    hardware/mcu/stm32f405/mcu.c
    hardware/mcu/stm32f405/mcu_timebase.c
    hardware/mcu/stm32f405/stm32f4xx_it.c
    hardware/mcu/stm32f405/system_clock.c
    hardware/mcu/stm32f405/timebase_snapshot.c
    "${STM32_DEVICE_ROOT}/Source/Templates/gcc/startup_stm32f405xx.s"
)

target_include_directories(flight_firmware PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/app"
    "${CMAKE_CURRENT_SOURCE_DIR}/hardware"
    "${CMAKE_CURRENT_SOURCE_DIR}/hardware/boards/flightcomputer_v1"
    "${CMAKE_CURRENT_SOURCE_DIR}/hardware/mcu/stm32f405"
    "${CMAKE_CURRENT_SOURCE_DIR}/peripherals/usb"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_link_libraries(flight_firmware PRIVATE stm32cube_f4)

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
