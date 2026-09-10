set(OFC_DEFAULT_CONFIGURATION_FILE
    "${PROJECT_SOURCE_DIR}/config/default-flight-configuration.json")
file(READ "${OFC_DEFAULT_CONFIGURATION_FILE}" OFC_DEFAULT_CONFIGURATION_JSON)

function(ofc_json_get output)
    string(JSON value ERROR_VARIABLE error GET
        "${OFC_DEFAULT_CONFIGURATION_JSON}" ${ARGN})
    if(error)
        message(FATAL_ERROR
            "Invalid default flight configuration at ${ARGN}: ${error}")
    endif()
    set(${output} "${value}" PARENT_SCOPE)
endfunction()

function(ofc_direction_constant output index)
    ofc_json_get(direction motors directions ${index})
    if(direction STREQUAL "NORMAL")
        set(value "MOTOR_DIRECTION_NORMAL")
    elseif(direction STREQUAL "REVERSED")
        set(value "MOTOR_DIRECTION_REVERSED")
    else()
        message(FATAL_ERROR "Invalid motor direction: ${direction}")
    endif()
    set(${output} "${value}" PARENT_SCOPE)
endfunction()

ofc_json_get(OFC_CONFIG_SCHEMA_VERSION schema_version)
if(NOT OFC_CONFIG_SCHEMA_VERSION EQUAL 3)
    message(FATAL_ERROR "Default configuration schema_version must be 3")
endif()
string(JSON direction_count LENGTH
    "${OFC_DEFAULT_CONFIGURATION_JSON}" motors directions)
if(NOT direction_count EQUAL 4)
    message(FATAL_ERROR "Default configuration requires exactly four motors")
endif()
ofc_json_get(propeller_layout motors propeller_layout)
if(propeller_layout STREQUAL "PROPS_IN")
    set(OFC_CONFIG_PROPELLER_LAYOUT "PROPELLER_LAYOUT_PROPS_IN")
elseif(propeller_layout STREQUAL "PROPS_OUT")
    set(OFC_CONFIG_PROPELLER_LAYOUT "PROPELLER_LAYOUT_PROPS_OUT")
else()
    message(FATAL_ERROR "Invalid propeller layout: ${propeller_layout}")
endif()
ofc_direction_constant(OFC_CONFIG_MOTOR_0 0)
ofc_direction_constant(OFC_CONFIG_MOTOR_1 1)
ofc_direction_constant(OFC_CONFIG_MOTOR_2 2)
ofc_direction_constant(OFC_CONFIG_MOTOR_3 3)
ofc_json_get(OFC_CONFIG_MIXER_ROLL mixer roll_factor)
ofc_json_get(OFC_CONFIG_MIXER_PITCH mixer pitch_factor)
ofc_json_get(OFC_CONFIG_MIXER_YAW mixer yaw_factor)
ofc_json_get(OFC_CONFIG_FAILSAFE_STALE receiver_failsafe stale_after_us)
ofc_json_get(OFC_CONFIG_FAILSAFE_LOSS receiver_failsafe loss_detected_after_us)
ofc_json_get(OFC_CONFIG_FAILSAFE_HOLD receiver_failsafe hold_last_until_us)
ofc_json_get(OFC_CONFIG_FAILSAFE_STAGE_TWO receiver_failsafe stage_two_after_us)
ofc_json_get(OFC_CONFIG_FAILSAFE_RECOVERY receiver_failsafe recovery_stable_us)
ofc_json_get(OFC_CONFIG_FAILSAFE_ROLL receiver_failsafe stage_one_roll)
ofc_json_get(OFC_CONFIG_FAILSAFE_PITCH receiver_failsafe stage_one_pitch)
ofc_json_get(OFC_CONFIG_FAILSAFE_YAW receiver_failsafe stage_one_yaw)
ofc_json_get(OFC_CONFIG_FAILSAFE_THROTTLE receiver_failsafe stage_one_throttle)
ofc_json_get(OFC_CONFIG_FAILSAFE_RECOVERY_THROTTLE
    receiver_failsafe recovery_throttle_maximum)
ofc_json_get(OFC_CONFIG_GYRO_SETTLING imu gyro_calibration settling_duration_us)
ofc_json_get(OFC_CONFIG_GYRO_SAMPLE imu gyro_calibration sample_duration_us)
ofc_json_get(OFC_CONFIG_GYRO_MAXIMUM_RATE imu gyro_calibration maximum_rate_dps)
ofc_json_get(OFC_CONFIG_GYRO_MAXIMUM_STANDARD_DEVIATION
    imu gyro_calibration maximum_standard_deviation_dps)
ofc_json_get(gyro_filter_type imu gyro_filter type)
if(gyro_filter_type STREQUAL "FIRST_ORDER_LOW_PASS")
    set(OFC_CONFIG_GYRO_FILTER_TYPE
        "FLIGHT_GYRO_FILTER_FIRST_ORDER_LOW_PASS")
else()
    message(FATAL_ERROR "Invalid gyro filter type: ${gyro_filter_type}")
endif()
ofc_json_get(OFC_CONFIG_GYRO_FILTER_CUTOFF imu gyro_filter cutoff_hz)
ofc_json_get(attitude_estimator_type imu attitude_estimator type)
if(attitude_estimator_type STREQUAL "COMPLEMENTARY")
    set(OFC_CONFIG_ATTITUDE_ESTIMATOR_TYPE
        "FLIGHT_ATTITUDE_ESTIMATOR_COMPLEMENTARY")
else()
    message(FATAL_ERROR
        "Invalid attitude estimator type: ${attitude_estimator_type}")
endif()
ofc_json_get(OFC_CONFIG_ACCELEROMETER_CORRECTION_TIME_CONSTANT
    imu attitude_estimator accelerometer_correction_time_constant_s)
ofc_json_get(OFC_CONFIG_ATTITUDE_MAXIMUM_GAP
    imu attitude_estimator maximum_gap_us)

foreach(factor IN ITEMS OFC_CONFIG_MIXER_ROLL OFC_CONFIG_MIXER_PITCH
                        OFC_CONFIG_MIXER_YAW)
    if(${factor} LESS 0 OR ${factor} GREATER 1)
        message(FATAL_ERROR "Default mixer factor ${factor} must be within 0..1")
    endif()
endforeach()
if(NOT OFC_CONFIG_FAILSAFE_STALE LESS OFC_CONFIG_FAILSAFE_LOSS OR
   NOT OFC_CONFIG_FAILSAFE_LOSS LESS OFC_CONFIG_FAILSAFE_HOLD OR
   NOT OFC_CONFIG_FAILSAFE_HOLD LESS OFC_CONFIG_FAILSAFE_STAGE_TWO OR
   OFC_CONFIG_FAILSAFE_RECOVERY LESS_EQUAL 0)
    message(FATAL_ERROR "Default receiver failsafe timing is invalid")
endif()
if(OFC_CONFIG_GYRO_FILTER_CUTOFF LESS_EQUAL 0 OR
   OFC_CONFIG_GYRO_FILTER_CUTOFF GREATER 500 OR
   OFC_CONFIG_ACCELEROMETER_CORRECTION_TIME_CONSTANT LESS_EQUAL 0 OR
   OFC_CONFIG_ACCELEROMETER_CORRECTION_TIME_CONSTANT GREATER 10 OR
   OFC_CONFIG_ATTITUDE_MAXIMUM_GAP LESS_EQUAL 0 OR
   OFC_CONFIG_ATTITUDE_MAXIMUM_GAP GREATER 1000000)
    message(FATAL_ERROR "Default IMU processing configuration is invalid")
endif()
if(OFC_CONFIG_GYRO_SETTLING LESS 0 OR
   OFC_CONFIG_GYRO_SETTLING GREATER 10000000 OR
   OFC_CONFIG_GYRO_SAMPLE LESS_EQUAL 0 OR
   OFC_CONFIG_GYRO_SAMPLE GREATER 10000000 OR
   OFC_CONFIG_GYRO_MAXIMUM_RATE LESS_EQUAL 0 OR
   OFC_CONFIG_GYRO_MAXIMUM_RATE GREATER 2000 OR
   OFC_CONFIG_GYRO_MAXIMUM_STANDARD_DEVIATION LESS_EQUAL 0 OR
   OFC_CONFIG_GYRO_MAXIMUM_STANDARD_DEVIATION GREATER 2000)
    message(FATAL_ERROR "Default gyro calibration configuration is invalid")
endif()

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/flight_configuration_defaults.h.in"
    "${CMAKE_BINARY_DIR}/generated/flight_configuration_defaults.h"
    @ONLY)
