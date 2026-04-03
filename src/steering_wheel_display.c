//
// Created by User on 02.04.2026.
//

#include "steering_wheel_display.h"

LOG_MODULE_REGISTER(steering_wheel_display);

static bool time_measurement_running = false;
static uint32_t lap_counter = 0;
static time_t time = { 0 };
volatile time_measurement_request_t time_measurement_request = IDLE;


static void format_mmss(char *buf, size_t len, uint64_t ms)
{
    uint32_t total_sec = (uint32_t)(ms / 1000);
    uint32_t minutes = total_sec / 60;
    uint32_t seconds = total_sec % 60;

    snprintf(buf, len, "%02u:%02u", minutes, seconds);
}

static void disp_set_time(uint64_t total_ms, uint64_t current_lap_ms)
{
    char buf_time[16];
    format_mmss(buf_time, sizeof(buf_time), total_ms);
    lv_textarea_set_text(objects.total_time_area, buf_time);

    format_mmss(buf_time, sizeof(buf_time), current_lap_ms);
    lv_textarea_set_text(objects.lap_time_area, buf_time);
}

static void disp_set_lap_number(uint8_t lap_number)
{
    char buf_lap_num[20];

    sprintf(buf_lap_num, "%u", lap_number);
    lv_textarea_set_text(objects.lap_number_area, buf_lap_num);
}

void disp_update_gui()
{
    uint64_t now = k_uptime_get();
    time.total_ms = 0;
    time.current_lap_ms = 0;

    if (time_measurement_running) {
        time.total_ms = now - time.race_start_ms;
        time.current_lap_ms = now - time.lap_start_ms;
    }

    disp_set_time(time.total_ms, time.current_lap_ms);
    disp_set_lap_number(lap_counter);
}

static void start_time_measurement()
{
    uint64_t now = k_uptime_get();

    time_measurement_running = true;
    lap_counter = 1;
    time.race_start_ms = now;
    time.lap_start_ms = now;

    LOG_INF("Time measuring started");
    disp_update_gui();
}

void next_lap_time_measurement()
{
    uint64_t now = k_uptime_get();

    if (!time_measurement_running) {
        start_time_measurement();
        return;
    }

    time.lap_duration_ms = now - time.lap_start_ms;

    if (lap_counter < MAX_LAPS) {
        time.laps_duration_ms[lap_counter] = time.lap_duration_ms;
        lap_counter++;
    }
    //char buf[16];
    //format_mmss(buf, sizeof(buf), lap_ms);
    //lv_label_set_text(label_last_lap, buf);

    LOG_INF("Lap %u", lap_counter);

    time.lap_start_ms = now;
    disp_update_gui();
}

void reset_time_measurements()
{
    time_measurement_running = false;
    lap_counter = 0;
    memset(&time, 0, sizeof(time));
    disp_update_gui();
    LOG_INF("Reset all");
    //lv_label_set_text(label_total, "00:00");
    //lv_label_set_text(label_lap, "00:00");
    //lv_label_set_text(label_last_lap, "--:--");
}

static void disp_set_vehicle_speed(uint8_t speed)
{
    char buffer_speed[20];

    sprintf(buffer_speed, "%u", speed);
    // lv_meter_set_indicator_value(objects.speed_meter, indicator1, speed);
    lv_textarea_set_text(objects.speed_area, buffer_speed);
}

static void disp_set_sc_voltage(uint8_t voltage)
{
    char buffer_sc_voltage[20];

    lv_bar_set_value(objects.sc_voltage_bar, voltage, LV_ANIM_ON);
    sprintf(buffer_sc_voltage, "%u V", voltage);
    lv_label_set_text(objects.sc_voltage, buffer_sc_voltage);
    if (voltage <= 40)
    {
        lv_obj_set_style_bg_color(
            objects.sc_voltage_bar, lv_color_hex(0xD71717), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }
    else
    {
        lv_obj_set_style_bg_color(
            objects.sc_voltage_bar, lv_color_hex(0xff2acf4f), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }
}

static void disp_set_message(char* msg, uint32_t color)
{
    lv_obj_set_style_bg_color(objects.messages_area, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_textarea_set_text(objects.messages_area, msg);
}

