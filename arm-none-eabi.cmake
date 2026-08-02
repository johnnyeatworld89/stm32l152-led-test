set(CMAKE_SYSTEM_NAME Generic)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(CMAKE_OBJCOPY arm-none-eabi-objcopy)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(MCPU cortex-m3)

set(CMAKE_C_FLAGS
    "-mcpu=${MCPU} -mthumb"
)

set(CMAKE_ASM_FLAGS
    "-mcpu=${MCPU} -mthumb"
)
