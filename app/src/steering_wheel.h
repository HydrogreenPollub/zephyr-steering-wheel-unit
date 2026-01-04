//
// Created by User on 28.12.2025.
//

#ifndef STEERING_WHEEL_H
#define STEERING_WHEEL_H

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include "gpio.h"
#include "can.h"
#include "can_ids.h"

#define DEBOUNCE_MS    20
#define CHECK_LIGHTS_BUTTONS_MS 100

typedef enum {
    ON  = 0,
    OFF = 1,
} button_state_t;

typedef enum {
    LIGHT_HAZARD             = 0,
    LIGHT_BEAM               = 1,
    LIGHT_POSITION           = 2,
    LIGHT_RIGHT_INDICATOR    = 3,
    LIGHT_LEFT_INDICATOR     = 4,
} light_bit_t;

typedef struct {
    struct gpio_dt_spec gpio;
    struct gpio_callback cb;
    struct k_work_delayable work;

    uint8_t stable_state;
    uint8_t light_bit;
}lights_button_t;

typedef struct {
    const struct device *can_device;
    struct gpio_dt_spec can_tx_led;
    struct gpio_dt_spec can_rx_led;
}swu_can_t;

void steering_wheel_init();

#endif //STEERING_WHEEL_H
