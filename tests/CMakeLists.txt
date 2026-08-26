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

add_executable(task_tests
    task_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/task.c"
)

target_include_directories(task_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
)

target_compile_options(task_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME task COMMAND task_tests)

add_executable(scheduler_tests
    scheduler_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/scheduler.c"
    "${PROJECT_SOURCE_DIR}/firmware/app/task.c"
)

target_include_directories(scheduler_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
)

target_compile_options(scheduler_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME scheduler COMMAND scheduler_tests)

add_executable(system_state_tests
    system_state_tests.c
    "${PROJECT_SOURCE_DIR}/firmware/app/system_state.c"
)

target_include_directories(system_state_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/firmware/app"
)

target_compile_options(system_state_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_test(NAME system_state COMMAND system_state_tests)
