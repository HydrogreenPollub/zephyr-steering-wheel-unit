cmake_minimum_required(VERSION 3.20.0)

if(NOT DEFINED BOARD)
    set(BOARD steering_wheel CACHE STRING "Zephyr target board")
    message(STATUS "BOARD not defined externally, using default: ${BOARD}")
endif()

if(NOT DEFINED BOARD_ROOT)
    set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR} CACHE PATH "Root directory for custom boards")
    message(STATUS "BOARD_ROOT not defined externally, using default: ${BOARD_ROOT}")
endif()

set(VEHICLE_TYPE "hydra" CACHE STRING "Steering wheel hardware variant")

if(VEHICLE_TYPE STREQUAL "hydra")
    set(DTC_OVERLAY_FILE
            "${CMAKE_CURRENT_SOURCE_DIR}/boards/hydrogreen/steering_wheel/steering_wheel_hydra.overlay"
    )
    set(VEHICLE_TYPE_DEFINE HYDRA)
elseif(VEHICLE_TYPE STREQUAL "hydros")
    set(DTC_OVERLAY_FILE
            "${CMAKE_CURRENT_SOURCE_DIR}/boards/hydrogreen/steering_wheel/steering_wheel_hydros.overlay"
    )
    set(VEHICLE_TYPE_DEFINE HYDROS)
else()
    message(FATAL_ERROR "Unknown VEHICLE_TYPE: ${VEHICLE_TYPE}")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app LANGUAGES C)

zephyr_compile_definitions(
        VEHICLE_TYPE=${VEHICLE_TYPE_DEFINE}
        LV_FONT_MONTSERRAT_18=1
        LV_FONT_MONTSERRAT_26=1
        LV_FONT_MONTSERRAT_30=1
)

target_sources(app PRIVATE
        src/main.c
        src/steering_wheel_inputs.c
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
