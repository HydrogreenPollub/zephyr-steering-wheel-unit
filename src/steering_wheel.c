//
// Created by User on 28.12.2025.
//

#include "steering_wheel.h"

LOG_MODULE_REGISTER(steering_wheel_unit);

static void ui_timer_cb(lv_timer_t *timer)
{
    ARG_UNUSED(timer);
    disp_update_gui();
}

static void input_cb(struct input_event *evt, void *user_data)
{
    ARG_UNUSED(user_data);

    if (evt->type != INPUT_EV_KEY || evt->value != 1) {
        return;
    }

	switch (evt->code) {
	 //case INPUT_KEY_0:
		//LOG_INF("time reset press");
		//break;
    case INPUT_KEY_A:
		LOG_INF("short press");
        time_measurement_request = START;
		break;
	case INPUT_KEY_X:
		LOG_INF("long press");
        time_measurement_request = RESET_;
    	break;
	default:
		break;
	}
}

INPUT_CALLBACK_DEFINE(NULL, input_cb, NULL);



int steering_wheel_init(){
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(display_dev)) {
        LOG_INF("Display nie jest gotowy");
        return 0;
    }
	LOG_INF("steering_wheel_init");

    display_blanking_off(display_dev);
	ui_init();
	lv_timer_create(ui_timer_cb, 100, NULL);


    while (1) {

        if(time_measurement_request == START){
            next_lap_time_measurement();
            time_measurement_request = IDLE;
        }
        if(time_measurement_request == RESET_){
            reset_time_measurements();
            time_measurement_request = IDLE;
        }
        lv_timer_handler();
        k_msleep(10);
    }

}

