cmake_minimum_required(VERSION 3.25)

project(
    openflightcomputer_firmware
    VERSION 0.1.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_subdirectory(firmware)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
