//
// Created by Dawid Pisarczyk on 28.12.2025.
//

#include "steering_wheel_inputs.h"

LOG_MODULE_REGISTER(steering_wheel_inputs);

struct k_mutex mcu_inputs_mutex;
static struct k_work mcu_inputs_can_send_work;

swu_button_t button = {
    .emergency = GPIO_DT_SPEC_GET(BUTTON_EMERGENCY_NODE, gpios),
    .deadman = GPIO_DT_SPEC_GET(BUTTON_DEADMAN_NODE, gpios),
    .start = GPIO_DT_SPEC_GET(BUTTON_START_NODE, gpios),
    .reset = GPIO_DT_SPEC_GET(BUTTON_RESET_NODE, gpios),

#if HYDROS == 0
    .pedals_calib = GPIO_DT_SPEC_GET(BUTTON_PEDALS_CALIB_NODE, gpios),
#endif
};

struct candef_mcu_inputs_t mcu_inputs_state = {
    .emergency_switch = BUTTON_RELEASED,
    .dead_mans_switch = BUTTON_RELEASED,
    .leakage_detected = 0,
    .shell_relay = 0,
    .start_button = BUTTON_RELEASED,
    .reset_button = BUTTON_RELEASED,
    .calibration_button = BUTTON_RELEASED,
    //.gas_pedal = 0,
};

static int check_button_gpios_ready()
{
    if (!gpio_is_ready_dt(&button.emergency)) {
        LOG_ERR("Emergency button GPIO not ready");
        return -ENODEV;
    }

    if (!gpio_is_ready_dt(&button.deadman)) {
        LOG_ERR("Deadman button GPIO not ready");
        return -ENODEV;
    }

    if (!gpio_is_ready_dt(&button.start)) {
        LOG_ERR("Start button GPIO not ready");
        return -ENODEV;
    }

    if (!gpio_is_ready_dt(&button.reset)) {
        LOG_ERR("Reset button GPIO not ready");
        return -ENODEV;
    }

    if (!gpio_is_ready_dt(&button.pedals_calib)) {
        LOG_ERR("Pedals calib button GPIO not ready");
        return -ENODEV;
    }

    return 0;
}

static uint8_t input_value_to_button_state(int32_t value)
{
    /*
     * Zephyr INPUT_EV_KEY:
     * value == 1 -> pressed
     * value == 0 -> released
     *
     * button logic:
     * 0 = button pressed / active state
     * 1 = button released / inactive state
     */
    return value ? BUTTON_PRESSED : BUTTON_RELEASED;
}

void read_all_buttons_from_gpio()
{
    int32_t button_state;

    k_mutex_lock(&mcu_inputs_mutex, K_FOREVER);

    button_state = gpio_pin_get_dt(&button.emergency);
    if (button_state >= 0) {
        mcu_inputs_state.emergency_switch = input_value_to_button_state(button_state);
    }

    button_state = gpio_pin_get_dt(&button.deadman);
    if (button_state >= 0) {
        mcu_inputs_state.dead_mans_switch = input_value_to_button_state(button_state);
    }

    button_state = gpio_pin_get_dt(&button.start);
    if (button_state >= 0) {
        mcu_inputs_state.start_button = input_value_to_button_state(button_state);
    }

    button_state = gpio_pin_get_dt(&button.reset);
    if (button_state >= 0) {
        mcu_inputs_state.reset_button = input_value_to_button_state(button_state);
    }

    button_state = gpio_pin_get_dt(&button.pedals_calib);
    if (button_state >= 0) {
        mcu_inputs_state.calibration_button = input_value_to_button_state(button_state);
    }

    k_mutex_unlock(&mcu_inputs_mutex);
}


static void mcu_inputs_can_send_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    int ret = can_send_mcu_inputs();
    if (ret < 0) {
        LOG_ERR("MCU_INPUTS CAN send failed: %d", ret);
        return;
    }
    if (k_sem_take(&can_tx_done_sem, K_MSEC(200)) != 0) {
        LOG_ERR("CAN TX timeout");
        return;
    }

    if (can_tx_result != 0) {
        LOG_ERR("CAN TX error: %d", can_tx_result);
        return;
    }

    LOG_INF("MCU_INPUTS sent via CAN succesfully");
    gpio_set(&can.tx_led);
    k_work_reschedule(&tx_led_off_work, K_MSEC(50));

}

static bool check_button_state(uint8_t *old_state, uint8_t new_state)
{
    if (*old_state == new_state) {
        return false;
    }

    *old_state = new_state;
    return true;
}

static bool update_buttons_states(uint16_t code, uint8_t state)
{
    bool button_state_changed = false;

    k_mutex_lock(&mcu_inputs_mutex, K_FOREVER);

    switch (code) {

    case INPUT_KEY_0:
        break;

    case INPUT_KEY_1:
        button_state_changed = check_button_state(&mcu_inputs_state.reset_button, state);
        break;

    case INPUT_KEY_2:
        button_state_changed = check_button_state(&mcu_inputs_state.start_button, state);
        break;

    case INPUT_KEY_3:
        button_state_changed = check_button_state(&mcu_inputs_state.emergency_switch, state);
        break;

    case INPUT_KEY_4:
        button_state_changed = check_button_state(&mcu_inputs_state.dead_mans_switch, state);
        break;

    case INPUT_KEY_5:
        button_state_changed = check_button_state(&mcu_inputs_state.calibration_button, state);
        break;

    case INPUT_KEY_6:
        break;

    default:
        break;
    }

    k_mutex_unlock(&mcu_inputs_mutex);

    return button_state_changed;
}

static void input_cb(struct input_event *evt, void *user_data)
{
    ARG_UNUSED(user_data);

    if (evt->type != INPUT_EV_KEY) {
        return;
    }

    if (evt->value == 1) {
        switch (evt->code) {
        case INPUT_KEY_A:
            LOG_INF("short press");
            time_measurement_request = START;
            return;

        case INPUT_KEY_X:
            LOG_INF("long press");
            time_measurement_request = RESET_;
            return;

        default:
            break;
        }
    }
#if HYDROS == 0
    if (evt->value != 0 && evt->value != 1) {
        return;
    }

    uint8_t state = input_value_to_button_state(evt->value);

    bool button_state_changed = update_buttons_states(evt->code, state);

    if (button_state_changed) {
        LOG_INF("Input changed: code=%u button_state=%u", evt->code, state);
        k_work_submit(&mcu_inputs_can_send_work);
    }
#endif
}

INPUT_CALLBACK_DEFINE(NULL, input_cb, NULL);


void swu_inputs_init()
{

    if (check_button_gpios_ready() < 0){
        return;
    }

    k_mutex_init(&mcu_inputs_mutex);
    k_work_init(&mcu_inputs_can_send_work, mcu_inputs_can_send_work_handler);

    read_all_buttons_from_gpio();
    k_work_submit(&mcu_inputs_can_send_work);

    LOG_INF("MCU inputs initialized");
}