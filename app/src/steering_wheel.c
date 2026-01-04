//
// Created by User on 28.12.2025.
//

#include "steering_wheel.h"

LOG_MODULE_REGISTER(steering_wheel_unit);

static uint8_t lights_buttons_mask;
static bool send_pending;

static lights_button_t lights_buttons[] = {
    { .gpio = GPIO_DT_SPEC_GET(DT_ALIAS(button_lights_hazard), gpios),
      .light_bit = LIGHT_HAZARD },
    { .gpio = GPIO_DT_SPEC_GET(DT_ALIAS(button_lights_beam), gpios),
      .light_bit = LIGHT_BEAM},
    { .gpio = GPIO_DT_SPEC_GET(DT_ALIAS(button_lights_position), gpios),
      .light_bit = LIGHT_POSITION },
    { .gpio = GPIO_DT_SPEC_GET(DT_ALIAS(button_lights_right_indicator), gpios),
      .light_bit = LIGHT_RIGHT_INDICATOR },
    { .gpio = GPIO_DT_SPEC_GET(DT_ALIAS(button_lights_left_indicator), gpios),
      .light_bit = LIGHT_LEFT_INDICATOR },
};

struct can_filter swu_filter[] = {
{.flags = 0U,
  .id = CAN_ID_SENSOR_SPEED,
  .mask = CAN_STD_ID_MASK
    },
{.flags = 0U,
   .id = CAN_ID_SC_VOLTAGE,
   .mask = CAN_STD_ID_MASK
    },
};

swu_can_t can = {
    .can_device = DEVICE_DT_GET(DT_ALIAS(can)),
    .can_rx_led = GPIO_DT_SPEC_GET(DT_ALIAS(can_rx_led), gpios),
};

static void lights_button_isr(const struct device *dev,
                       struct gpio_callback *cb,
                       uint32_t pins)
{
    lights_button_t *btn =
        CONTAINER_OF(cb, lights_button_t, cb);

    k_work_reschedule(&btn->work, K_MSEC(DEBOUNCE_MS));
}

static void lights_button_work_handler(struct k_work *work)
{
    struct k_work_delayable *dwork =
        k_work_delayable_from_work(work);

    lights_button_t *btn =
        CONTAINER_OF(dwork, lights_button_t, work);

    int val = gpio_pin_get_dt(&btn->gpio);

    if (val != btn->stable_state) {
        btn->stable_state = val;

        if (val) {
            lights_buttons_mask |= BIT(btn->light_bit);
        } else {
            lights_buttons_mask &= ~BIT(btn->light_bit);
        }

        send_pending = true;
    }
}

static void lights_buttons_init()
{
    for (int i = 0; i < ARRAY_SIZE(lights_buttons); i++) {
        lights_button_t *btn = &lights_buttons[i];

        gpio_init(&btn->gpio, GPIO_INPUT);

        gpio_set_interrupt(&btn->gpio, GPIO_INT_EDGE_BOTH, &btn->cb,  lights_button_isr);

        btn->stable_state = gpio_pin_get_dt(&btn->gpio);

        k_work_init_delayable(&btn->work, lights_button_work_handler);
    }
}

void lcu_can_rx_callback(const struct device *dev, struct can_frame *frame, void *user_data) {
    if ((frame -> id) == 0x11) {
        printf("Odebrano: %u\n", frame->data[0]);

    }
}


static void swu_can_init() {
    can_init(can.can_device, HYDROGREEN_CAN_BAUD_RATE);

    for (int i = 0; i < ARRAY_SIZE(swu_filter); i++) {
        can_add_rx_filter_(can.can_device, lcu_can_rx_callback, &swu_filter[i]);
    }
}

static void system_timer_handler(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    for (int i = 0; i < ARRAY_SIZE(lights_buttons); i++) {
        k_work_submit(&lights_buttons[i].work.work);
    }
    send_pending = true;
}

K_TIMER_DEFINE(system_timer, system_timer_handler, NULL);

void steering_wheel_init() {
    lights_buttons_init();
    swu_can_init();

    k_timer_start(&system_timer,
              K_MSEC(CHECK_LIGHTS_BUTTONS_MS),
              K_MSEC(CHECK_LIGHTS_BUTTONS_MS));


}

void can_tx_thread(void)
{
    while (1) {
        if (send_pending) {
            send_pending = false;
            can_send_(can.can_device, CAN_ID_BUTTONS_LIGHTS_MASK, &lights_buttons_mask, sizeof(lights_buttons_mask) );
        }
        k_sleep(K_MSEC(10));
    }
}

