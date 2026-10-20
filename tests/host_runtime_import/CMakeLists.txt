cmake_minimum_required(VERSION 3.25)

project(openflightcomputer_host_runtime_import LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT must identify the firmware repository")
endif()

add_subdirectory(
    "${SOURCE_ROOT}/firmware/host"
    openflightcomputer-host
)

add_executable(host_runtime_import main.c)
target_link_libraries(host_runtime_import PRIVATE
    OpenFlightComputer::host_runtime
)
