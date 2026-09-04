cmake_minimum_required(VERSION 3.25)

project(
    openflightcomputer_firmware
    VERSION 0.1.0
    LANGUAGES C ASM
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(
    OPENFLIGHTCOMPUTER_BUILD_FIRMWARE
    "Build the STM32F405 Flight Computer V1 firmware image"
    ${CMAKE_CROSSCOMPILING}
)

add_custom_target(check_motor_architecture ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${PROJECT_SOURCE_DIR}/tests/check_motor_boundaries.cmake"
    COMMENT "Checking motor submission and arm-event boundaries"
    VERBATIM
)

add_subdirectory(firmware/common)

if(OPENFLIGHTCOMPUTER_BUILD_FIRMWARE)
    add_subdirectory(firmware)
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
