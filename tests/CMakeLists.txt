add_executable(timebase_snapshot_tests
    timebase_snapshot_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/hardware/mcu/stm32f405/timebase_snapshot.c"
)

target_include_directories(timebase_snapshot_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/hardware/mcu/stm32f405"
)

target_compile_options(timebase_snapshot_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME timebase_snapshot COMMAND timebase_snapshot_tests)
