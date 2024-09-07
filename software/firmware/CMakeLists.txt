cmake_minimum_required(VERSION 3.8)

project(microgame_fw C CXX ASM)

set(CROSS_COMPILE avr-)

# SimAVR doesn't like debug firmware
set(CMAKE_C_FLAGS_DEBUG "") 
set(CMAKE_C_FLAGS_RELEASE "")

set(CMAKE_ASM_COMPILER ${CROSS_COMPILE}gcc)
set(CMAKE_C_COMPILER ${CROSS_COMPILE}gcc)
set(CMAKE_CXX_COMPILER ${CROSS_COMPILE}g++)
set(CMAKE_OBJCOPY ${CROSS_COMPILE}objcopy)
set(CMAKE_OBJDUMP ${CROSS_COMPILE}objdump)

set(AVR_MCU attiny85)

add_compile_options(
    -mmcu=${AVR_MCU} 
    -Wall -gdwarf-2 -Os -std=gnu99 
    -fno-inline-small-functions
    -ffunction-sections -fdata-sections
)

add_link_options(
    -mmcu=${AVR_MCU} 
    -Wl,--relax,--gc-sections
    -Wl,--undefined=_mmcu,--section-start=.mmcu=0x910000
)

include_directories(${PROJECT_SRC_DIR})

add_executable(microgame.axf
    main.c
)

add_custom_command(
    TARGET microgame.axf POST_BUILD
    COMMAND ${CMAKE_OBJDUMP} --disassemble-all --source microgame.axf > microgame_dump.txt
)
