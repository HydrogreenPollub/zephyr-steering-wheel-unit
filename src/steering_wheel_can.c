//
// Created by User on 21.03.2026.
//

#include "steering_wheel_can.h"

LOG_MODULE_REGISTER(swu_can, LOG_LEVEL_INF);

#define SWU_CAN_TX_THREAD_STACK_SIZE       2048
#define SWU_CAN_TX_THREAD_PRIORITY         5
#define SWU_DISPLAY_THREAD_STACK_SIZE      2048
#define SWU_DISPLAY_THREAD_PRIORITY        5

K_THREAD_STACK_DEFINE(swu_can_tx_thread_stack_area, SWU_CAN_TX_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(swu_display_thread_stack_area, SWU_DISPLAY_THREAD_STACK_SIZE);

struct k_thread swu_can_tx_thread_data;
struct k_thread swu_display_thread_data;

K_SEM_DEFINE(can_tx_done_sem, 0, 1);
K_MSGQ_DEFINE(can_tx_msgq, sizeof(struct can_frame), 32, 4);

static volatile int can_tx_result;
static struct k_work_delayable tx_led_off_work;
static struct k_work_delayable rx_led_off_work;
static struct k_work_delayable bus_off_recovery_work;

struct can_filter swu_can_filter[] = {
    CAN_FILTER_EXT(CANDEF_MCU_ANALOG_POWERTRAIN_FRAME_ID),
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

    LOG_INF("CAN ID: 0x%03X, Data: %u", frame->id, frame->data[0]);

    switch ((can_id_t)frame->id) {
        case CAN_ID_BRAKE_PEDAL_VOLTAGE:
            LOG_INF("BRAKE_PEDAL_VOLTAGE");
            break;
        case CAN_ID_BUTTONS_LIGHTS_MASK:
            break;
        default:
            break;
    }
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

static void swu_can_tx_thread(void *p1, void *p2, void *p3) {
    struct can_frame frame = {0};
    LOG_INF("CAN TX thread started");

    while (1) {
        k_msgq_get(&can_tx_msgq, &frame, K_FOREVER);

        int ret = can_send(can.device, &frame, K_MSEC(100), swu_can_tx_callback, NULL);
        if (ret) {
            LOG_ERR("CAN send failed: %d", ret);
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

        gpio_set(&can.tx_led);
        k_work_reschedule(&tx_led_off_work, K_MSEC(50));
    }
}

static void swu_display_thread(void *p1, void *p2, void *p3) {

    while (1) {
        k_sleep(K_MSEC(10));
    }
}

static void swu_can_tx_callback(const struct device *dev, int error, void *user_data) {
    can_tx_result = error;
    k_sem_give(&can_tx_done_sem);
}

    void swu_can_init(void) {
        can_init(can.device, HYDROGREEN_CAN_BAUD_RATE);
        gpio_init(&can.rx_led, GPIO_OUTPUT_INACTIVE);
        gpio_init(&can.tx_led, GPIO_OUTPUT_INACTIVE);
        k_work_init_delayable(&tx_led_off_work, tx_led_off_handler);
        k_work_init_delayable(&rx_led_off_work, rx_led_off_handler);
        k_work_init_delayable(&bus_off_recovery_work, bus_off_recovery_handler);

        for(size_t i = 0; i < ARRAY_SIZE(swu_can_filter); i++) {
            can_add_rx_filter_(can.device, swu_can_rx_callback, &swu_can_filter[i]);
        }
        k_tid_t tx_tid = k_thread_create(
            &swu_can_tx_thread_data, swu_can_tx_thread_stack_area,
            K_THREAD_STACK_SIZEOF(swu_can_tx_thread_stack_area),
            swu_can_tx_thread, NULL, NULL, NULL,
            SWU_CAN_TX_THREAD_PRIORITY, 0, K_NO_WAIT);
        k_thread_name_set(tx_tid, "can_tx");

        k_tid_t display_tid = k_thread_create(
            &swu_display_thread_data, swu_display_thread_stack_area,
            K_THREAD_STACK_SIZEOF(swu_display_thread_stack_area),
            swu_display_thread, NULL, NULL, NULL,
            SWU_DISPLAY_THREAD_PRIORITY, 0, K_NO_WAIT);
        k_thread_name_set(display_tid, "display");
    }
