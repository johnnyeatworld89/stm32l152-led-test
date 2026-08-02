
cmake_minimum_required(VERSION 3.20)

project(firmware C ASM)

set(CMAKE_C_STANDARD 11)

set(MCU_FAMILY STM32L1xx)

set(SOURCES
    Core/Src/main.c
    Core/Src/system_stm32l1xx.c

    Drivers/STM32L1xx_HAL_Driver/Src/stm32l1xx_hal.c
    Drivers/STM32L1xx_HAL_Driver/Src/stm32l1xx_hal_gpio.c
    Drivers/STM32L1xx_HAL_Driver/Src/stm32l1xx_hal_rcc.c
)

add_executable(firmware.elf
    ${SOURCES}
    startup/startup_stm32l152xe.s
)

target_include_directories(firmware.elf PRIVATE
    Core/Inc

    Drivers/CMSIS/Include
    Drivers/CMSIS/Device/ST/STM32L1xx/Include

    Drivers/STM32L1xx_HAL_Driver/Inc
)

target_compile_definitions(firmware.elf PRIVATE
    STM32L152xE
    USE_HAL_DRIVER
)

target_compile_options(firmware.elf PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -Os
)

target_link_options(firmware.elf PRIVATE
    -T${CMAKE_SOURCE_DIR}/linker/STM32L152RETx_FLASH.ld
    -mcpu=cortex-m3
    -mthumb
    --specs=nano.specs
    -Wl,--gc-sections
)

set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=firmware.map"
)
