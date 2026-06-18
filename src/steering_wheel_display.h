//
// Created by User on 02.04.2026.
//

#ifndef STEERING_WHEEL_DISPLAY_H
#define STEERING_WHEEL_DISPLAY_H

#include <lvgl.h>
#include "ui.h"
#include <zephyr/logging/log.h>
#include "steering_wheel_inputs.h"
#include "steering_wheel_can.h"

#define MAX_LAPS 50

typedef struct
{
    uint64_t total_ms;
    uint64_t total_s;
    uint64_t current_lap_ms;
    uint64_t current_lap_s;
    uint64_t race_start_ms;
    uint64_t lap_start_ms;
    uint64_t lap_start_s;
    uint64_t laps_duration_ms[MAX_LAPS];
    uint64_t lap_duration_ms;
    bool measurement_running;
} time_t;

typedef enum
{
    START,
    IDLE,
    RESET_,
}time_measurement_request_t;

typedef struct
{
    uint16_t speed_kph;
    uint16_t sc_voltage;
    struct candef_mcu_faults_t mcu_faults;
}display_data_t;

extern display_data_t display_data;
extern volatile time_measurement_request_t time_measurement_request;
extern uint32_t lap_counter;
extern time_t time_g;
void reset_time_measurements();
void next_lap_time_measurement();
void disp_update_gui();
void swu_display_init();



#endif //STEERING_WHEEL_DISPLAY_H
