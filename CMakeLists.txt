cmake_minimum_required(VERSION 3.22)

project(stm32l152-led-test C ASM)

set(CMAKE_C_STANDARD 11)

enable_language(ASM)

# STM32 HAL sources
file(GLOB HAL_SOURCES
    Drivers/STM32L1xx_HAL_Driver/Src/*.c
)

# CMSIS sources
set(CMSIS_SOURCES
    Drivers/CMSIS/Device/ST/STM32L1xx/Source/Templates/system_stm32l1xx.c
)

add_executable(firmware.elf
    Core/Src/main.c
    Core/Src/stm32l1xx_it.c
    Core/Src/system_stm32l1xx.c
    startup/startup_stm32l152xe.s
    ${HAL_SOURCES}
)

target_include_directories(firmware.elf PRIVATE
    Core/Inc
    Drivers/STM32L1xx_HAL_Driver/Inc
    Drivers/CMSIS/Include
    Drivers/CMSIS/Device/ST/STM32L1xx/Include
)

target_compile_definitions(firmware.elf PRIVATE
    STM32L152xE
    USE_HAL_DRIVER
)

target_compile_options(firmware.elf PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -Wall
)

target_link_options(firmware.elf PRIVATE
    -T${CMAKE_SOURCE_DIR}/linker/STM32L152RETx_FLASH.ld
    -mcpu=cortex-m3
    -mthumb
    --specs=nano.specs
    -Wl,--gc-sections
)

set_target_properties(firmware.elf PROPERTIES
    SUFFIX ".elf"
)

add_custom_command(TARGET firmware.elf POST_BUILD
    COMMAND arm-none-eabi-objcopy -O binary
    ${CMAKE_BINARY_DIR}/firmware.elf
    ${CMAKE_BINARY_DIR}/firmware.bin
)

