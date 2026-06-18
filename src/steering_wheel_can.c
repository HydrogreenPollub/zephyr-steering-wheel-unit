//
// Created by Dawid Pisarczyk on 21.03.2026.
//

#include "steering_wheel_can.h"

LOG_MODULE_REGISTER(swu_can, LOG_LEVEL_INF);

#define SWU_CAN_TX_THREAD_STACK_SIZE                   2048
#define SWU_CAN_TX_THREAD_PRIORITY                     5
#define SWU_CAN_PROCESS_RX_DATA_THREAD_STACK_SIZE      2048
#define SWU_CAN_PROCESS_RX_DATA_THREAD_PRIORITY        5

K_THREAD_STACK_DEFINE(swu_can_tx_thread_stack_area, SWU_CAN_TX_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(swu_can_process_rx_data_thread_stack_area, SWU_CAN_PROCESS_RX_DATA_THREAD_STACK_SIZE);

struct k_thread swu_can_tx_thread_data;
struct k_thread swu_can_process_rx_data_thread_data;

struct k_sem can_tx_done_sem;
K_SEM_DEFINE(can_tx_done_sem, 0, 1);
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

int can_send_mcu_inputs()
{
    struct candef_mcu_inputs_t inputs;
    uint8_t data[CANDEF_MCU_INPUTS_LENGTH] = { 0 };

    k_mutex_lock(&mcu_inputs_mutex, K_FOREVER);
    inputs = mcu_inputs_state;
    k_mutex_unlock(&mcu_inputs_mutex);

    candef_mcu_inputs_pack(data, &inputs, sizeof(data));

    struct can_frame frame = {
        .id = CANDEF_MCU_INPUTS_FRAME_ID,
        .dlc = CANDEF_MCU_INPUTS_LENGTH,
        .flags = CAN_FRAME_IDE,
    };

    memcpy(frame.data, data, CANDEF_MCU_INPUTS_LENGTH);

    int ret = can_send(can.device, &frame, K_MSEC(100), swu_can_tx_callback, NULL);

    return ret;
}

int can_send_mcu_time()
{
    struct candef_swu_time_t mcu_time;
    uint8_t time_data[CANDEF_SWU_TIME_LENGTH] = { 0 };

    struct candef_swu_state_t mcu_state;
    uint8_t lap_data[CANDEF_SWU_STATE_LENGTH] = { 0 };


    //k_mutex_lock(&mcu_inputs_mutex, K_FOREVER);
    mcu_time.total_time_ms = time_g.total_ms;
    mcu_time.lap_time_ms = time_g.current_lap_ms;
    mcu_state.lap_number = lap_counter;
    //k_mutex_unlock(&mcu_inputs_mutex);

    candef_swu_time_pack(time_data, &mcu_time, sizeof(time_data));
    candef_swu_state_pack(lap_data, &mcu_state, sizeof(lap_data));

    struct can_frame time_frame = {
        .id = CANDEF_SWU_TIME_FRAME_ID,
        .dlc = CANDEF_SWU_TIME_LENGTH,
        .flags = CAN_FRAME_IDE,
    };
    struct can_frame lap_frame = {
        .id = CANDEF_SWU_STATE_FRAME_ID,
        .dlc = CANDEF_SWU_STATE_LENGTH,
        .flags = CAN_FRAME_IDE,
    };

    memcpy(time_frame.data, time_data, CANDEF_SWU_TIME_LENGTH);
    memcpy(lap_frame.data, lap_data, CANDEF_SWU_STATE_LENGTH);

    can_send(can.device, &time_frame, K_MSEC(100), swu_can_tx_callback, NULL);
    int ret = can_send(can.device, &lap_frame, K_MSEC(100), swu_can_tx_callback, NULL);

    return ret;
}

static void swu_can_tx_thread(void *p1, void *p2, void *p3) {

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    //struct can_frame frame = {0};
    LOG_INF("MCU inputs heartbeat can transmit thread started");

    while (1) {
        //k_msgq_get(&can_tx_msgq, &frame, K_FOREVER);
        read_all_buttons_from_gpio();

        int ret = can_send_mcu_inputs();
        can_send_mcu_time();
        /*if (ret < 0) {
            LOG_ERR("MCU_INPUTS CAN send failed: %d", ret);
            continue;
        }

        if (k_sem_take(&can_tx_done_sem, K_MSEC(200)) != 0) {
            LOG_ERR("CAN TX timeout");
            continue;
        }

        if (can_tx_result != 0) {
            LOG_ERR("CAN TX error: %d", can_tx_result);
            continue;
        }
*/
        LOG_INF("MCU_INPUTS sent via CAN succesfully");
        gpio_set(&can.tx_led);
        k_work_reschedule(&tx_led_off_work, K_MSEC(50));

        k_msleep(MCU_INPUTS_HEARTBEAT_MS);
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

/*   case CANDEF_MCU_ANALOG_POWERTRAIN_FRAME_ID: {
        if (frame->dlc >= CANDEF_MCU_ANALOG_POWERTRAIN_LENGTH) {
           struct candef_mcu_analog_powertrain_t dst_p;
            candef_mcu_analog_powertrain_unpack(&dst_p, frame->data, frame->dlc);

            k_mutex_lock(&can_data_mutex, K_FOREVER);
            display_data.sc_voltage = dst_p.supercapacitor_voltage;
            k_mutex_unlock(&can_data_mutex);
        }
        break;*/
	case CANDEF_MCU_ANALOG_FUEL_CELL_FRAME_ID: {
        if (frame->dlc >= CANDEF_MCU_ANALOG_FUEL_CELL_LENGTH) {
           struct candef_mcu_analog_fuel_cell_t dst_p;
            candef_mcu_analog_fuel_cell_unpack(&dst_p, frame->data, frame->dlc);

            k_mutex_lock(&can_data_mutex, K_FOREVER);
            display_data.sc_voltage = dst_p.fuel_cell_output_voltage;
            k_mutex_unlock(&can_data_mutex);
        }
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

#if HYDROS == 0
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
#endif

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
