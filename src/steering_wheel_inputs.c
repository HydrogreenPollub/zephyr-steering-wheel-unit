//
// Created by Dawid Pisarczyk on 28.12.2025.
//

#include "steering_wheel_inputs.h"

LOG_MODULE_REGISTER(steering_wheel_inputs);

K_MUTEX_DEFINE(mcu_inputs_mutex);

swu_button_t button = {
    .emergency = GPIO_DT_SPEC_GET(BUTTON_EMERGENCY_NODE, gpios),
    .deadman = GPIO_DT_SPEC_GET(BUTTON_DEADMAN_NODE, gpios),
    .start = GPIO_DT_SPEC_GET(BUTTON_START_NODE, gpios),
    .reset = GPIO_DT_SPEC_GET(BUTTON_RESET_NODE, gpios),

#if VEHICLE_TYPE == HYDRA
    .pedals_calib = GPIO_DT_SPEC_GET(BUTTON_PEDALS_CALIB_NODE, gpios),
    .light_hazard = GPIO_DT_SPEC_GET(BUTTON_LIGHT_HAZARD_NODE, gpios),
    .light_beam = GPIO_DT_SPEC_GET(BUTTON_LIGHT_BEAM_NODE, gpios),
    .light_position = GPIO_DT_SPEC_GET(BUTTON_LIGHT_POSITION_NODE, gpios),
    .light_right_indicator = GPIO_DT_SPEC_GET(BUTTON_LIGHT_RIGHT_INDICATOR_NODE, gpios),
    .light_left_indicator = GPIO_DT_SPEC_GET(BUTTON_LIGHT_LEFT_INDICATOR_NODE, gpios),
#endif
};

struct candef_swu_mcu_inputs_t mcu_inputs_state = {
    .emergency_switch = BUTTON_RELEASED,
    .dead_mans_switch = BUTTON_RELEASED,
    .leakage_detected = 0,
    .shell_relay = 0,
    .start_button = BUTTON_RELEASED,
    .reset_button = BUTTON_RELEASED,
    .calibration_button = BUTTON_RELEASED,
};

struct candef_swu_lcu_inputs_t lcu_inputs_state = { 0 };

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

#if VEHICLE_TYPE == HYDRA
    if (!gpio_is_ready_dt(&button.pedals_calib)) {
        LOG_ERR("Pedals calib button GPIO not ready");
        return -ENODEV;
    }

    if (!gpio_is_ready_dt(&button.light_hazard)) {
        LOG_ERR("Light hazard button GPIO not ready");
        return -ENODEV;
    }

    if (!gpio_is_ready_dt(&button.light_beam)) {
        LOG_ERR("Light beam button GPIO not ready");
        return -ENODEV;
    }

    if (!gpio_is_ready_dt(&button.light_position)) {
        LOG_ERR("Light position button GPIO not ready");
        return -ENODEV;
    }

    if (!gpio_is_ready_dt(&button.light_right_indicator)) {
        LOG_ERR("Light right indicator button GPIO not ready");
        return -ENODEV;
    }

    if (!gpio_is_ready_dt(&button.light_left_indicator)) {
        LOG_ERR("Light left indicator button GPIO not ready");
        return -ENODEV;
    }
#endif

    return 0;
}

static uint8_t input_value_to_button_state(int32_t value)
{
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

#if VEHICLE_TYPE == HYDRA
    button_state = gpio_pin_get_dt(&button.pedals_calib);
    if (button_state >= 0) {
        mcu_inputs_state.calibration_button = input_value_to_button_state(button_state);
    }

    button_state = gpio_pin_get_dt(&button.light_hazard);
    if (button_state >= 0) {
        lcu_inputs_state.hazard = input_value_to_button_state(button_state);
    }

    button_state = gpio_pin_get_dt(&button.light_beam);
    if (button_state >= 0) {
        lcu_inputs_state.beam = input_value_to_button_state(button_state);
    }

    button_state = gpio_pin_get_dt(&button.light_position);
    if (button_state >= 0) {
        lcu_inputs_state.position = input_value_to_button_state(button_state);
    }

    button_state = gpio_pin_get_dt(&button.light_right_indicator);
    if (button_state >= 0) {
        lcu_inputs_state.right_indicator = input_value_to_button_state(button_state);
    }

    button_state = gpio_pin_get_dt(&button.light_left_indicator);
    if (button_state >= 0) {
        lcu_inputs_state.left_indicator = input_value_to_button_state(button_state);
    }
#endif

    k_mutex_unlock(&mcu_inputs_mutex);
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

static bool update_lcu_button_states(uint16_t code, uint8_t state)
{
    bool button_state_changed = false;

#if VEHICLE_TYPE == HYDRA
    k_mutex_lock(&mcu_inputs_mutex, K_FOREVER);

    switch (code) {

    case INPUT_KEY_7:
        button_state_changed = check_button_state(&lcu_inputs_state.hazard, state);
        break;

    case INPUT_KEY_8:
        button_state_changed = check_button_state(&lcu_inputs_state.beam, state);
        break;

    case INPUT_KEY_9:
        button_state_changed = check_button_state(&lcu_inputs_state.position, state);
        break;

    case INPUT_KEY_B:
        button_state_changed = check_button_state(&lcu_inputs_state.right_indicator, state);
        break;

    case INPUT_KEY_C:
        button_state_changed = check_button_state(&lcu_inputs_state.left_indicator, state);
        break;

    default:
        break;
    }

    k_mutex_unlock(&mcu_inputs_mutex);
#else
    ARG_UNUSED(code);
    ARG_UNUSED(state);
#endif

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
    if (evt->value != 0 && evt->value != 1) {
        return;
    }

    uint8_t state = input_value_to_button_state(evt->value);

    bool button_state_changed = update_buttons_states(evt->code, state);
    button_state_changed |= update_lcu_button_states(evt->code, state);

    if (button_state_changed) {
        LOG_INF("Input changed: code=%u button_state=%u", evt->code, state);
        swu_can_request_tx();
    }
}

INPUT_CALLBACK_DEFINE(NULL, input_cb, NULL);


void swu_inputs_init()
{

    if (check_button_gpios_ready() < 0){
        return;
    }

    read_all_buttons_from_gpio();

    LOG_INF("MCU inputs initialized");
}
