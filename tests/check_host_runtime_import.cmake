if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED IMPORT_BINARY_ROOT)
    message(FATAL_ERROR
        "SOURCE_ROOT and IMPORT_BINARY_ROOT must both be provided")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${SOURCE_ROOT}/tests/host_runtime_import"
        -B "${IMPORT_BINARY_ROOT}"
        --fresh
        "-DSOURCE_ROOT=${SOURCE_ROOT}"
    RESULT_VARIABLE configure_result
)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "Nested host-runtime configuration failed: ${configure_result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        --build "${IMPORT_BINARY_ROOT}"
        --target host_runtime_import
    RESULT_VARIABLE build_result
)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR
        "Nested host-runtime build failed: ${build_result}")
endif()
