//
// Created by User on 21.03.2026.
//

#ifndef STEERING_WHEEL_CAN_H
#define STEERING_WHEEL_CAN_H

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include "gpio.h"
#include "can.h"
#include "can_ids.h"
#include "candef.h"

typedef struct {
    const struct device *device;
    struct gpio_dt_spec tx_led;
    struct gpio_dt_spec rx_led;
}swu_can_t;




#endif //STEERING_WHEEL_CAN_H
