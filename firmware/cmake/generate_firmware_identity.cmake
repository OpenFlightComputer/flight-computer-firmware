if(NOT DEFINED SOURCE_DIRECTORY OR
   NOT DEFINED FIRMWARE_VERSION OR
   NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "Firmware identity generation is missing an input.")
endif()

execute_process(
    COMMAND git rev-parse --short=7 HEAD
    WORKING_DIRECTORY "${SOURCE_DIRECTORY}"
    RESULT_VARIABLE git_hash_result
    OUTPUT_VARIABLE git_hash
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT git_hash_result EQUAL 0 OR git_hash STREQUAL "")
    set(git_hash "unknown")
endif()

execute_process(
    COMMAND git status --porcelain --untracked-files=normal -- .
    WORKING_DIRECTORY "${SOURCE_DIRECTORY}"
    RESULT_VARIABLE git_status_result
    OUTPUT_VARIABLE git_status
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
set(dirty_suffix "")
if(NOT git_status_result EQUAL 0)
    set(dirty_suffix "-unknown")
elseif(NOT git_status STREQUAL "")
    set(dirty_suffix "-dirty")
endif()

set(build_id "${git_hash}${dirty_suffix}")
set(version_string "v${FIRMWARE_VERSION}+git.${git_hash}")
if(dirty_suffix STREQUAL "-dirty")
    string(APPEND version_string ".dirty")
elseif(dirty_suffix STREQUAL "-unknown")
    string(APPEND version_string ".unknown")
endif()

set(content
"#include \"firmware_identity.h\"\n\nconst char firmware_version[] = \"${FIRMWARE_VERSION}\";\nconst char firmware_build_id[] = \"${build_id}\";\nconst char firmware_version_string[] = \"${version_string}\";\n")

get_filename_component(output_directory "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
set(write_output TRUE)
if(EXISTS "${OUTPUT_FILE}")
    file(READ "${OUTPUT_FILE}" existing_content)
    if(existing_content STREQUAL content)
        set(write_output FALSE)
    endif()
endif()
if(write_output)
    file(WRITE "${OUTPUT_FILE}" "${content}")
endif()
