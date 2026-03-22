//
// Created by User on 28.12.2025.
//

#include "steering_wheel.h"

LOG_MODULE_REGISTER(steering_wheel_unit);

static lv_obj_t *label_title;
static lv_obj_t *label_counter;
static uint32_t seconds;

static void counter_timer_cb(lv_timer_t *timer)
{
    ARG_UNUSED(timer);

    static char buf[64];
    seconds++;

    snprintk(buf, sizeof(buf), "Czas pracy: %u s", seconds);
    lv_label_set_text(label_counter, buf);
    lv_obj_align(label_counter, LV_ALIGN_CENTER, 0, 30);
}

int steering_wheel_init(){
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(display_dev)) {
        printk("Display nie jest gotowy\n");
        return 0;
    }

    display_blanking_off(display_dev);

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

    label_title = lv_label_create(lv_scr_act());
    lv_label_set_text(label_title, "Hydrogreen / Zephyr / LVGL");
    lv_obj_set_style_text_color(label_title, lv_color_white(), 0);
    lv_obj_align(label_title, LV_ALIGN_CENTER, 0, -10);

    label_counter = lv_label_create(lv_scr_act());
    lv_label_set_text(label_counter, "Czas pracy: 0 s");
    lv_obj_set_style_text_color(label_counter, lv_color_white(), 0);
    lv_obj_align(label_counter, LV_ALIGN_CENTER, 0, 30);

    lv_timer_create(counter_timer_cb, 1000, NULL);

    while (1) {
        lv_timer_handler();
        k_msleep(10);
    }

}

