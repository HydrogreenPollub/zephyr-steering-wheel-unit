//
// Created by User on 28.12.2025.
//

#ifndef STEERING_WHEEL_H
#define STEERING_WHEEL_H

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


int steering_wheel_init();

#endif //STEERING_WHEEL_H
