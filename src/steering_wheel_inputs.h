//
// Created by Dawid Pisarczyk on 28.12.2025.
//

#ifndef STEERING_WHEEL_INPUTS_H
#define STEERING_WHEEL_INPUTS_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/display.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <lvgl.h>
#include "gpio.h"
#include "can.h"
#include "can_ids.h"
#include "ui.h"
#include "steering_wheel_display.h"
#include "steering_wheel_can.h"

#define BUTTON_EMERGENCY_NODE     DT_NODELABEL(button_emergency)
#define BUTTON_DEADMAN_NODE       DT_NODELABEL(button_deadman)
#define BUTTON_START_NODE         DT_NODELABEL(button_start)
#define BUTTON_RESET_NODE         DT_NODELABEL(button_reset)
#define BUTTON_LAP_TIME_NODE      DT_NODELABEL(button_lap_time)
#define BUTTON_PEDALS_CALIB_NODE  DT_NODELABEL(button_pedals_calib)
#define BUTTON_TEST_NODE          DT_NODELABEL(button_test)

#define MCU_INPUTS_HEARTBEAT_MS       500

/*
 * button logic:
 * 0 = button pressed / active state
 * 1 = button released / inactive state
 */
typedef enum {
    BUTTON_PRESSED  = 0,
    BUTTON_RELEASED = 1,
} button_state_t;

typedef struct {
    struct gpio_dt_spec emergency;
    struct gpio_dt_spec deadman;
    struct gpio_dt_spec start;
    struct gpio_dt_spec reset;
    struct gpio_dt_spec pedals_calib;
}swu_button_t;

extern struct k_mutex mcu_inputs_mutex;
extern swu_button_t button;
extern struct candef_mcu_inputs_t mcu_inputs_state;

void read_all_buttons_from_gpio();
void swu_inputs_init();

#define HYDROS 0
#define HYDRA  1

#endif //STEERING_WHEEL_INPUTS_H
