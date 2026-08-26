set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(ARM_GNU_TOOLCHAIN_ROOT
    ""
    CACHE PATH
    "Optional root of a complete Arm GNU toolchain installation"
)

set(ARM_GNU_TOOLCHAIN_HINTS
    "${ARM_GNU_TOOLCHAIN_ROOT}/bin"
    "$ENV{HOME}/.local/share/OpenFlightComputer/arm-gnu-toolchain-15.3.rel1/bin"
    "/Applications/ArmGNUToolchain/15.3.rel1/arm-none-eabi/bin"
)

find_program(CMAKE_C_COMPILER arm-none-eabi-gcc
    HINTS ${ARM_GNU_TOOLCHAIN_HINTS}
    REQUIRED
)
find_program(CMAKE_ASM_COMPILER arm-none-eabi-gcc
    HINTS ${ARM_GNU_TOOLCHAIN_HINTS}
    REQUIRED
)
find_program(CMAKE_AR arm-none-eabi-ar HINTS ${ARM_GNU_TOOLCHAIN_HINTS} REQUIRED)
find_program(CMAKE_OBJCOPY arm-none-eabi-objcopy
    HINTS ${ARM_GNU_TOOLCHAIN_HINTS}
    REQUIRED
)
find_program(CMAKE_OBJDUMP arm-none-eabi-objdump
    HINTS ${ARM_GNU_TOOLCHAIN_HINTS}
    REQUIRED
)
find_program(CMAKE_RANLIB arm-none-eabi-ranlib
    HINTS ${ARM_GNU_TOOLCHAIN_HINTS}
    REQUIRED
)
find_program(CMAKE_SIZE arm-none-eabi-size
    HINTS ${ARM_GNU_TOOLCHAIN_HINTS}
    REQUIRED
)

set(STM32_CPU_FLAGS
    "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard"
)

set(CMAKE_C_FLAGS_INIT
    "${STM32_CPU_FLAGS} -ffunction-sections -fdata-sections -fno-common"
)
set(CMAKE_ASM_FLAGS_INIT "${STM32_CPU_FLAGS} -x assembler-with-cpp")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${STM32_CPU_FLAGS}")
