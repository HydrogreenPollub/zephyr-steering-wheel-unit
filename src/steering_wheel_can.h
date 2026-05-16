//
// Created by Dawid Pisarczyk on 21.03.2026.
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
#include "steering_wheel_display.h"
#include "steering_wheel_inputs.h"

typedef struct {
    const struct device *device;
    struct gpio_dt_spec tx_led;
    struct gpio_dt_spec rx_led;
}swu_can_t;

extern struct k_mutex can_data_mutex;
extern struct k_sem can_tx_done_sem;
extern volatile int can_tx_result;
extern struct k_work_delayable tx_led_off_work;

extern swu_can_t can;

void swu_can_init();
int can_send_mcu_inputs();


#endif //STEERING_WHEEL_CAN_H
