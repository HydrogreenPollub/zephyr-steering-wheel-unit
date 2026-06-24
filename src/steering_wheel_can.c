//
// Created by Dawid Pisarczyk on 21.03.2026.
//

#include "steering_wheel_can.h"

LOG_MODULE_REGISTER(swu_can, LOG_LEVEL_INF);

#define SWU_CAN_TX_THREAD_STACK_SIZE                   2048
#define SWU_CAN_TX_THREAD_PRIORITY                     5
#define SWU_CAN_PROCESS_RX_DATA_THREAD_STACK_SIZE      2048
#define SWU_CAN_PROCESS_RX_DATA_THREAD_PRIORITY        5
#define SWU_CAN_HEARTBEAT_MS                           500
#define SWU_STATUS_TIME_MAX_MS                         0xFFFFFFu
#define SWU_STATUS_LAP_NUMBER_MAX                      0xFFu

K_THREAD_STACK_DEFINE(swu_can_tx_thread_stack_area, SWU_CAN_TX_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(swu_can_process_rx_data_thread_stack_area, SWU_CAN_PROCESS_RX_DATA_THREAD_STACK_SIZE);

struct k_thread swu_can_tx_thread_data;
struct k_thread swu_can_process_rx_data_thread_data;

K_SEM_DEFINE(can_tx_done_sem, 0, 1);
K_SEM_DEFINE(can_tx_request_sem, 0, 1);
//K_MSGQ_DEFINE(can_tx_msgq, sizeof(struct can_frame), 32, 4);
K_MSGQ_DEFINE(can_rx_msgq, sizeof(struct can_frame), 32, 4);
struct k_mutex can_data_mutex;

volatile int can_tx_result;
struct k_work_delayable tx_led_off_work;
static struct k_work_delayable rx_led_off_work;
static struct k_work_delayable bus_off_recovery_work;

struct can_filter swu_can_filter[] = {
    //CAN_FILTER_EXT(CANDEF_MCU_ANALOG_POWERTRAIN_FRAME_ID),
    CAN_FILTER_EXT(CANDEF_MCU_ANALOG_FUEL_CELL_FRAME_ID),
    CAN_FILTER_EXT(CANDEF_MCU_ANALOG_DRIVE_FRAME_ID),
    CAN_FILTER_EXT(CANDEF_MCU_FAULTS_FRAME_ID),
};

swu_can_t can = {
    .device = DEVICE_DT_GET(DT_ALIAS(can)),
    .rx_led = GPIO_DT_SPEC_GET(DT_ALIAS(can_rx_led), gpios),
    .tx_led = GPIO_DT_SPEC_GET(DT_ALIAS(can_tx_led), gpios),
};

static void swu_can_tx_callback(const struct device *dev, int error, void *user_data);

static uint8_t saturate_u8(uint32_t value, uint8_t max)
{
    return value > max ? max : (uint8_t)value;
}

static uint32_t saturate_u24(uint64_t value)
{
    return value > SWU_STATUS_TIME_MAX_MS ? SWU_STATUS_TIME_MAX_MS : (uint32_t)value;
}

static int send_swu_frame(uint32_t id, uint8_t dlc, const uint8_t *data)
{
    struct can_frame frame = {
        .id = id,
        .dlc = dlc,
        .flags = CAN_FRAME_IDE,
    };

    memcpy(frame.data, data, dlc);

    return can_send(can.device, &frame, K_MSEC(100), swu_can_tx_callback, NULL);
}

static void drain_tx_requests()
{
    while (k_sem_take(&can_tx_request_sem, K_NO_WAIT) == 0) {
    }
}

static void tx_led_off_handler(struct k_work *work) { gpio_reset(&can.tx_led); }
static void rx_led_off_handler(struct k_work *work) { gpio_reset(&can.rx_led); }

static void swu_can_rx_callback(const struct device *dev, struct can_frame *frame, void *user_data) {
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);

    gpio_set(&can.rx_led);
    k_work_reschedule(&rx_led_off_work, K_MSEC(50));

    LOG_INF("CAN ID: 0x%03X", frame->id);

    k_msgq_put(&can_rx_msgq, frame, K_NO_WAIT);
}

static void bus_off_recovery_handler(struct k_work *work) {
    int ret = can_recover(can.device, K_MSEC(100));
    if (ret != 0 && ret != -ENOTSUP) {
        LOG_WRN("CAN recovery failed (%d), retrying in 1s", ret);
        k_work_reschedule(&bus_off_recovery_work, K_SECONDS(1));
    } else {
        LOG_INF("CAN bus recovered");
        // status_led_set(STATUS_LED_OPERATIONAL);
    }
}

void swu_can_request_tx()
{
    k_sem_give(&can_tx_request_sem);
}

int can_send_swu_status()
{
    enum can_state can_state = CAN_STATE_STOPPED;
    struct candef_swu_status_t swu_status = { 0 };
    uint8_t data[CANDEF_SWU_STATUS_LENGTH] = { 0 };
    int ret;

    ret = can_get_state(can.device, &can_state, NULL);
    swu_status.can_state = ret < 0 ? UINT8_MAX : (uint8_t)can_state;
    swu_status.lap_number = saturate_u8(lap_counter, SWU_STATUS_LAP_NUMBER_MAX);
    swu_status.lap_time_ms = saturate_u24(time_g.current_lap_ms);
    swu_status.total_time_ms = saturate_u24(time_g.total_ms);

    ret = candef_swu_status_pack(data, &swu_status, sizeof(data));
    if (ret < 0) {
        return ret;
    }

    return send_swu_frame(CANDEF_SWU_STATUS_FRAME_ID, CANDEF_SWU_STATUS_LENGTH, data);
}

int can_send_swu_mcu_inputs()
{
    struct candef_swu_mcu_inputs_t swu_mcu_inputs = { 0 };
    uint8_t data[CANDEF_SWU_MCU_INPUTS_LENGTH] = { 0 };
    int ret;

    k_mutex_lock(&mcu_inputs_mutex, K_FOREVER);
    swu_mcu_inputs = mcu_inputs_state;
    k_mutex_unlock(&mcu_inputs_mutex);

    ret = candef_swu_mcu_inputs_pack(data, &swu_mcu_inputs, sizeof(data));
    if (ret < 0) {
        return ret;
    }

    return send_swu_frame(CANDEF_SWU_MCU_INPUTS_FRAME_ID, CANDEF_SWU_MCU_INPUTS_LENGTH, data);
}

int can_send_swu_lcu_inputs()
{
    struct candef_swu_lcu_inputs_t swu_lcu_inputs = { 0 };
    uint8_t data[CANDEF_SWU_LCU_INPUTS_LENGTH] = { 0 };
    int ret;

    k_mutex_lock(&mcu_inputs_mutex, K_FOREVER);
    swu_lcu_inputs = lcu_inputs_state;
    k_mutex_unlock(&mcu_inputs_mutex);

    ret = candef_swu_lcu_inputs_pack(data, &swu_lcu_inputs, sizeof(data));
    if (ret < 0) {
        return ret;
    }

    return send_swu_frame(CANDEF_SWU_LCU_INPUTS_FRAME_ID, CANDEF_SWU_LCU_INPUTS_LENGTH, data);
}

static void swu_can_tx_thread(void *p1, void *p2, void *p3) {

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    //struct can_frame frame = {0};
    LOG_INF("SWU CAN heartbeat transmit thread started");

    while (1) {
        //k_msgq_get(&can_tx_msgq, &frame, K_FOREVER);
        read_all_buttons_from_gpio();

        int ret = can_send_swu_status();
        if (ret < 0) {
            LOG_ERR("SWU_STATUS CAN send failed: %d", ret);
        }

        ret = can_send_swu_mcu_inputs();
        if (ret < 0) {
            LOG_ERR("SWU_MCU_INPUTS CAN send failed: %d", ret);
        }

        ret = can_send_swu_lcu_inputs();
        if (ret < 0) {
            LOG_ERR("SWU_LCU_INPUTS CAN send failed: %d", ret);
        }
        LOG_INF("SWU heartbeat frames sent via CAN successfully");
        gpio_set(&can.tx_led);
        k_work_reschedule(&tx_led_off_work, K_MSEC(50));

        drain_tx_requests();
        k_sem_take(&can_tx_request_sem, K_MSEC(SWU_CAN_HEARTBEAT_MS));
    }
}

static void swu_can_tx_callback(const struct device *dev, int error, void *user_data) {
    can_tx_result = error;
    k_sem_give(&can_tx_done_sem);
}

static void process_can_frame(const struct can_frame *frame)
{
    switch (frame->id) {
    case CANDEF_MCU_ANALOG_DRIVE_FRAME_ID: {
        if (frame->dlc >= CANDEF_MCU_ANALOG_DRIVE_LENGTH) {
            struct candef_mcu_analog_drive_t dst_p;
            candef_mcu_analog_drive_unpack(&dst_p, frame->data, frame->dlc);

            k_mutex_lock(&can_data_mutex, K_FOREVER);
            display_data.speed_kph = dst_p.speed;
            k_mutex_unlock(&can_data_mutex);
        }
        break;
    }

    case CANDEF_MCU_ANALOG_FUEL_CELL_FRAME_ID: {
        if (frame->dlc >= CANDEF_MCU_ANALOG_FUEL_CELL_LENGTH) {
           struct candef_mcu_analog_fuel_cell_t dst_p;
            candef_mcu_analog_fuel_cell_unpack(&dst_p, frame->data, frame->dlc);

            k_mutex_lock(&can_data_mutex, K_FOREVER);
            display_data.sc_voltage = dst_p.fuel_cell_output_voltage;
            k_mutex_unlock(&can_data_mutex);
        }
        break;
    }
    case CANDEF_MCU_FAULTS_FRAME_ID: {
       if (frame->dlc >= CANDEF_MCU_FAULTS_LENGTH) {
           struct candef_mcu_faults_t dst_p;
           candef_mcu_faults_unpack(&dst_p, frame->data, frame->dlc);

           k_mutex_lock(&can_data_mutex, K_FOREVER);
           display_data.mcu_faults = dst_p;
           k_mutex_unlock(&can_data_mutex);
       }
       break;
    }

    default:
       break;
    }
}

static void swu_can_process_rx_data_thread(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    LOG_INF("CAN process frames thread started");

    struct can_frame frame;
    while (1) {
        if (k_msgq_get(&can_rx_msgq, &frame, K_FOREVER) == 0) {
            process_can_frame(&frame);
        }
    }
}

void swu_can_init() {
    can_init(can.device, HYDROGREEN_CAN_BAUD_RATE);
    gpio_init(&can.rx_led, GPIO_OUTPUT_INACTIVE);
    gpio_init(&can.tx_led, GPIO_OUTPUT_INACTIVE);
    k_work_init_delayable(&tx_led_off_work, tx_led_off_handler);
    k_work_init_delayable(&rx_led_off_work, rx_led_off_handler);
    k_work_init_delayable(&bus_off_recovery_work, bus_off_recovery_handler);

    for(size_t i = 0; i < ARRAY_SIZE(swu_can_filter); i++) {
        can_add_rx_filter_(can.device, swu_can_rx_callback, &swu_can_filter[i]);
    }

    k_mutex_init(&can_data_mutex);

    k_tid_t can_tx_tid = k_thread_create(
        &swu_can_tx_thread_data,
        swu_can_tx_thread_stack_area,
        K_THREAD_STACK_SIZEOF(swu_can_tx_thread_stack_area),
        swu_can_tx_thread,
        NULL,
        NULL,
        NULL,
        SWU_CAN_TX_THREAD_PRIORITY,
        0,
        K_NO_WAIT);

    k_thread_name_set(can_tx_tid, "can_tx");

    k_tid_t can_process_rx_data_tid = k_thread_create(
        &swu_can_process_rx_data_thread_data,
        swu_can_process_rx_data_thread_stack_area,
        K_THREAD_STACK_SIZEOF(swu_can_process_rx_data_thread_stack_area),
        swu_can_process_rx_data_thread,
        NULL,
        NULL,
        NULL,
        SWU_CAN_PROCESS_RX_DATA_THREAD_PRIORITY,
        0,
        K_NO_WAIT);

    k_thread_name_set(can_process_rx_data_tid, "can_process_rx_data");
}
