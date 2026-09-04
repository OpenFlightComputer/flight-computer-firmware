if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(GLOB_RECURSE PRODUCTION_SOURCES
    RELATIVE "${SOURCE_ROOT}"
    "${SOURCE_ROOT}/firmware/*.c"
    "${SOURCE_ROOT}/firmware/*.h"
)
list(FILTER PRODUCTION_SOURCES EXCLUDE REGEX "^firmware/third_party/")

function(assert_token_is_private token)
    set(allowed_files ${ARGN})

    foreach(source_file IN LISTS PRODUCTION_SOURCES)
        file(READ "${SOURCE_ROOT}/${source_file}" contents)
        string(FIND "${contents}" "${token}" token_position)
        if(token_position EQUAL -1)
            continue()
        endif()

        set(is_allowed FALSE)
        foreach(allowed_file IN LISTS allowed_files)
            if(source_file STREQUAL allowed_file)
                set(is_allowed TRUE)
            endif()
        endforeach()

        if(NOT is_allowed)
            message(FATAL_ERROR
                "Motor boundary violation: ${source_file} uses ${token}"
            )
        endif()
    endforeach()
endfunction()

set(MOTOR_OUTPUT_FILES
    firmware/app/motor_control.c
    firmware/flight/actuators/motor_output.c
    firmware/flight/actuators/motor_output.h
)

assert_token_is_private("motor_output_t" ${MOTOR_OUTPUT_FILES})
assert_token_is_private("motor_output_initialize" ${MOTOR_OUTPUT_FILES})
assert_token_is_private("motor_output_submit" ${MOTOR_OUTPUT_FILES})
assert_token_is_private("motor_output_force_stop" ${MOTOR_OUTPUT_FILES})

set(DSHOT_ENCODER_FILES
    firmware/peripherals/dshot/dshot_encoder.c
    firmware/peripherals/dshot/dshot_encoder.h
)

assert_token_is_private("dshot_encode_throttle" ${DSHOT_ENCODER_FILES})
assert_token_is_private("dshot_encode_command" ${DSHOT_ENCODER_FILES})

set(DSHOT_TIMING_FILES
    firmware/peripherals/dshot/dshot_timing.c
    firmware/peripherals/dshot/dshot_timing.h
)

assert_token_is_private("dshot_timing_profile_create" ${DSHOT_TIMING_FILES})
assert_token_is_private("dshot_timing_build_dma_buffer" ${DSHOT_TIMING_FILES})

set(ARM_EVENT_FILES
    firmware/app/system_state.c
    firmware/app/system_state.h
    firmware/app/usb_command_processor.c
)

assert_token_is_private("SYSTEM_STATE_EVENT_ARM_REQUESTED" ${ARM_EVENT_FILES})
