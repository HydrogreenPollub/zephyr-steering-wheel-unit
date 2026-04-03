cmake_minimum_required(VERSION 3.13.1)

if(NOT DEFINED BOARD)
    set(BOARD steering_wheel CACHE STRING "Zephyr target board")
    message(STATUS "BOARD not defined externally, using default: ${BOARD}")
endif()

if(NOT DEFINED BOARD_ROOT)
    set(BOARD_ROOT . CACHE PATH "Root directory for custom boards")
    message(STATUS "BOARD_ROOT not defined externally, using default: ${BOARD_ROOT}")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app LANGUAGES C)

target_sources(app PRIVATE
        src/main.c
        src/steering_wheel.c
        src/steering_wheel_can.c
        src/steering_wheel_display.c
        peripherals/can.c
#        peripherals/can_dfu.c
        peripherals/gpio.c
        peripherals/adc.c
        peripherals/pwm.c
        peripherals/counter.c
        peripherals/uart.c
        peripherals/rs485.c
        peripherals/status_led.c
        peripherals/test_button.c
        can-definitions/candef.c
        ui/ui.c
        ui/screens.c
        ui/styles.c
)

target_include_directories(app PRIVATE
        peripherals
        can-definitions
        src
        ui
)