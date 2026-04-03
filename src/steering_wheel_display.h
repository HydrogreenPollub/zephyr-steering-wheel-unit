//
// Created by User on 02.04.2026.
//

#ifndef STEERING_WHEEL_DISPLAY_H
#define STEERING_WHEEL_DISPLAY_H

#include <lvgl.h>
#include "ui.h"
#include <zephyr/logging/log.h>

#define MAX_LAPS 50

typedef struct
{
    uint64_t total_ms;
    uint64_t current_lap_ms;
    uint64_t race_start_ms;
    uint64_t lap_start_ms;
    uint64_t laps_duration_ms[MAX_LAPS];
    uint64_t lap_duration_ms;
} time_t;

typedef enum
{
    START,
    IDLE,
    RESET_,
}time_measurement_request_t;

extern volatile time_measurement_request_t time_measurement_request;

void reset_time_measurements();
void next_lap_time_measurement();
void disp_update_gui();



#endif //STEERING_WHEEL_DISPLAY_H
