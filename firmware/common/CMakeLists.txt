add_library(firmware_common STATIC
    uint64_decimal.c
)

target_include_directories(firmware_common PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_options(firmware_common PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)
