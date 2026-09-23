set(expected_configuration
    "${SOURCE_ROOT}/config/default-flight-configuration.json")
set(OFC_DEFAULT_CONFIGURATION_FILE "${expected_configuration}")
set(CMAKE_BINARY_DIR "${IMPORT_BINARY_ROOT}")

include("${SOURCE_ROOT}/cmake/FlightConfiguration.cmake")

if(NOT OFC_DEFAULT_CONFIGURATION_FILE STREQUAL expected_configuration)
    message(FATAL_ERROR
        "FlightConfiguration.cmake replaced the caller's configuration path")
endif()
if(NOT EXISTS
   "${IMPORT_BINARY_ROOT}/generated/flight_configuration_defaults.h")
    message(FATAL_ERROR
        "FlightConfiguration.cmake did not generate the defaults header")
endif()
