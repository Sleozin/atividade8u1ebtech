# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(atividade8u1ebtech C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(atividade8u1ebtech atividade8u1ebtech.c 
inc/display_utils.c
inc/big_string_drawer.c
inc/ssd1306_i2c.c
inc/font_big_logo_data.c)

pico_set_program_name(atividade8u1ebtech "atividade8u1ebtech")
pico_set_program_version(atividade8u1ebtech "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(atividade8u1ebtech 0)
pico_enable_stdio_usb(atividade8u1ebtech 1)

# Add the standard library to the build
target_link_libraries(atividade8u1ebtech
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(atividade8u1ebtech PRIVATE
        ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/inc
)

# Add any user requested libraries
target_link_libraries(atividade8u1ebtech 
        hardware_dma
        hardware_timer
        hardware_pwm
        hardware_i2c
        pico_cyw43_arch_lwip_threadsafe_background
        )

pico_add_extra_outputs(atividade8u1ebtech)

