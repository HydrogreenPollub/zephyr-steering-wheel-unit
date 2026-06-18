//
// Created by Dawid Pisarczyk on 02.04.2026.
//

#include "steering_wheel_display.h"

LOG_MODULE_REGISTER(steering_wheel_display);

#define OK_COLOR    0x00ff00
#define ERROR_COLOR 0xff0000

#define SWU_DISPLAY_THREAD_STACK_SIZE      4096
#define SWU_DISPLAY_THREAD_PRIORITY        5

K_THREAD_STACK_DEFINE(swu_display_thread_stack_area, SWU_DISPLAY_THREAD_STACK_SIZE);
struct k_thread swu_display_thread_data;

uint32_t lap_counter = 0;
time_t time_g = { 0 };
display_data_t display_data = { 0 };
volatile time_measurement_request_t time_measurement_request = IDLE;

static inline lv_color_t my_color_hex(uint32_t hex)
{
    uint32_t r = (hex >> 16) & 0xFF;
    uint32_t g = (hex >> 8)  & 0xFF;
    uint32_t b = hex & 0xFF;

    return lv_color_hex((b << 16) | (g << 8) | r);
}

static const char *mcu_faults_get_message(const struct candef_mcu_faults_t *f)
{
    if (f->emergency_switch) return "EMERGENCY SWITCH PRESSED";
    if (f->emergency_dead_mans_switch) return "DEAD MAN";
    if (f->emergency_leakage_detected) return "H2 LEAKAGE DETECTED";

    if (f->error_fc_v_low) return "FC VOLTAGE TOO LOW";
    if (f->error_fc_v_high) return "FC VOLTAGE TOO HIGH";
    if (f->error_fc_c_low) return "FC CURRENT TOO LOW";
    if (f->error_fc_c_high) return "FC CURRENT TOO HIGH";

    if (f->error_sc_v_low) return "SC VOLTAGE TOO LOW";
    if (f->error_sc_v_high) return "SC VOLTAGE TOO HIGH";
    if (f->error_sc_c_low) return "SC CURRENT TOO LOW";
    if (f->error_sc_c_high) return "SC CURRENT TOO HIGH";

    if (f->error_mc_v_low) return "MC VOLTAGE TOO LOW";
    if (f->error_mc_v_high) return "MC VOLTAGE TOO HIGH";
    if (f->error_mc_c_low) return "MC CURRENT TOO LOW";
    if (f->error_mc_c_high) return "MC CURRENT TOO HIGH";

    if (f->error_ab_v_low) return "AB VOLTAGE TOO LOW";
    if (f->error_ab_v_high) return "AB VOLTAGE TOO HIGH";
    if (f->error_ab_c_low) return "AB CURRENT TOO LOW";
    if (f->error_ab_c_high) return "AB CURRENT TOO HIGH";

    return NULL;
}

static void format_mmss(char *buf, size_t len, uint64_t total_sec)
{
    //uint32_t total_sec = ms / 1000;
    uint32_t minutes = total_sec / 60;
    uint32_t seconds = total_sec % 60;
    //uint32_t centis = (ms % 1000) / 10;

    snprintf(buf, len, "%u:%02u", minutes, seconds);
//.%02u
}

static void disp_set_time(uint64_t total_s, uint64_t current_lap_s)
{
    char buf_time[16];
    format_mmss(buf_time, sizeof(buf_time), total_s);
    lv_textarea_set_text(objects.total_time_area, buf_time);

    format_mmss(buf_time, sizeof(buf_time), current_lap_s);
    lv_textarea_set_text(objects.lap_time_area, buf_time);
}

static void disp_set_lap_number(uint8_t lap_number)
{
    char buf_lap_num[20];

    sprintf(buf_lap_num, "%u", lap_number);
    lv_textarea_set_text(objects.lap_number_area, buf_lap_num);
}

static void start_time_measurement()
{
    uint64_t now = k_uptime_get();

    time_g.measurement_running = true;
    lap_counter = 1;

    time_g.race_start_ms = now;
    time_g.lap_start_ms = now;

    time_g.total_ms = 0;
    time_g.current_lap_ms = 0;
    time_g.lap_duration_ms = 0;

    time_g.lap_start_s = 0;

    LOG_INF("Time measuring started");
    //disp_update_gui();
}

void next_lap_time_measurement()
{
    uint64_t now = k_uptime_get();

    if (!time_g.measurement_running) {
        start_time_measurement();
        return;
    }

    time_g.lap_duration_ms = now - time_g.lap_start_ms;

    if (lap_counter <= MAX_LAPS) {
        time_g.laps_duration_ms[lap_counter - 1] = time_g.lap_duration_ms;
    }

    if (lap_counter < MAX_LAPS) {
        lap_counter++;
    }
    //char buf[16];
    //format_mmss(buf, sizeof(buf), lap_ms);
    //lv_label_set_text(label_last_lap, buf);
    time_g.lap_start_ms = now;
    time_g.lap_start_s = (uint32_t)((now - time_g.race_start_ms) / 1000);

    LOG_INF("Lap %u", lap_counter);

    //disp_update_gui();
}

void reset_time_measurements()
{
    lap_counter = 0;
    memset(&time_g, 0, sizeof(time_g));
    //disp_update_gui();
    LOG_INF("Reset all");
    //lv_label_set_text(label_total, "00:00");
    //lv_label_set_text(label_lap, "00:00");
    //lv_label_set_text(label_last_lap, "--:--");
}

static void disp_set_vehicle_speed(uint16_t speed)
{
    char buffer_speed[20];
	speed /= 100;

    sprintf(buffer_speed, "%u", speed);
    // lv_meter_set_indicator_value(objects.speed_meter, indicator1, speed);
    lv_textarea_set_text(objects.speed_area, buffer_speed);
}

static void disp_set_sc_voltage(uint16_t supercap_voltage)
{
    char buffer_sc_voltage[20];
	//voltage /= 1000;
	uint16_t voltage = supercap_voltage / 1000;
	uint16_t voltage_dec_part = supercap_voltage %= 1000;
    lv_bar_set_value(objects.sc_voltage_bar, voltage, LV_ANIM_ON);
    sprintf(buffer_sc_voltage, "%u.%.2u V", voltage, voltage_dec_part);
    lv_label_set_text(objects.sc_voltage, buffer_sc_voltage);
    if (voltage <= 40)
    {
        lv_obj_set_style_bg_color(
            objects.sc_voltage_bar, my_color_hex(ERROR_COLOR), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }
    else
    {
        lv_obj_set_style_bg_color(
            objects.sc_voltage_bar, my_color_hex(OK_COLOR), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }
}

static void disp_set_message(char* msg, uint32_t color)
{
    lv_obj_set_style_bg_color(objects.messages_area, my_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_textarea_set_text(objects.messages_area, msg);
}

void disp_update_gui()
{
    uint64_t now = k_uptime_get();

    const char *fault_msg = NULL;

    time_g.total_ms = 0;
    time_g.total_s = 0;

    time_g.current_lap_ms = 0;
    time_g.current_lap_s = 0;

    display_data_t data = { 0 };

    k_mutex_lock(&can_data_mutex, K_FOREVER);
    data = display_data;
    k_mutex_unlock(&can_data_mutex);

    if (time_g.measurement_running) {
        time_g.total_ms = now - time_g.race_start_ms;
        time_g.current_lap_ms = now - time_g.lap_start_ms;

        time_g.total_s = (uint32_t)(time_g.total_ms / 1000);

        if (time_g.total_s >= time_g.lap_start_s) {
            time_g.current_lap_s = time_g.total_s - time_g.lap_start_s;
        } else {
            time_g.current_lap_s = 0;
        }
    }

    fault_msg = mcu_faults_get_message(&data.mcu_faults);

    disp_set_time(time_g.total_s, time_g.current_lap_s);
    disp_set_lap_number(lap_counter);
    disp_set_vehicle_speed(data.speed_kph);
    disp_set_sc_voltage(data.sc_voltage);

    if (fault_msg != NULL) {
        disp_set_message((char *)fault_msg, ERROR_COLOR);
    } else {
        disp_set_message("EVERYTHING OK - NO FAULTS", OK_COLOR);
    }
}

static void swu_display_thread(void *p1, void *p2, void *p3) {

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    LOG_INF("Display thread started");

    while (1) {
        if(time_measurement_request == START){
            next_lap_time_measurement();
            time_measurement_request = IDLE;
        }
        if(time_measurement_request == RESET_){
            reset_time_measurements();
            time_measurement_request = IDLE;
        }
        disp_update_gui();
        lv_timer_handler();
        k_msleep(10);
    }
}

static void ui_timer_cb(lv_timer_t *timer)
{
    ARG_UNUSED(timer);
    disp_update_gui();
}

void swu_display_init()
{
    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(display)) {
        LOG_ERR("Display is not ready");
        return;
    }

    display_blanking_off(display);
    ui_init();
    //lv_timer_create(ui_timer_cb, 100, NULL);

    k_tid_t display_tid = k_thread_create(
        &swu_display_thread_data,
        swu_display_thread_stack_area,
        K_THREAD_STACK_SIZEOF(swu_display_thread_stack_area),
        swu_display_thread,
        NULL,
        NULL,
        NULL,
        SWU_DISPLAY_THREAD_PRIORITY,
        0,
        K_NO_WAIT
    );
    k_thread_name_set(display_tid, "display");

    LOG_INF("Display initialized");
}
