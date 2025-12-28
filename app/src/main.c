// #include <zephyr/kernel.h>
// #include <zephyr/drivers/gpio.h>
// #include <zephyr/drivers/adc.h>
// #include <zephyr/devicetree.h>
// #include <stdio.h> // Include for printk
// #include <zephyr/logging/log.h>
//
// #if !DT_NODE_HAS_STATUS(DT_ALIAS(ledgreen), okay)
// #error "No LED node has been configured"
// #endif
//
// // #if !DT_NODE_HAS_STATUS(DT_ALIAS(button), okay)
// // #error "No button node has been configured"
// // #endif
//
// // #if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
// // #error "No ADC I/O node has been configured"
// // #endif
//
// #define SLEEP_TIME_MS 1000
//
// LOG_MODULE_REGISTER(app);
//
// static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(ledgreen), gpios);
// // static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(button), gpios);
// // static const struct adc_dt_spec adc_channel_idx0 = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);
//
// // static struct gpio_callback button_cb_data;
//
// // uint16_t adc_sample;
// // struct adc_sequence adc_sequence = {
// //     .buffer = &adc_sample,
// //     .buffer_size = sizeof(adc_sample), /* buffer size in bytes, not number of samples */
// // };
//
// int led_initialize() {
//     if (!device_is_ready(led.port)) {
//         // Use device_is_ready for driver check
//         printk("Error: GPIO device %s is not ready\n", led.port->name);
//         return 1; // Indicate error
//     } else {
//         printk("GPIO device %s is ready\n", led.port->name);
//     }
//
//     int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
//     if (ret < 0) {
//         printk("Error %d: failed to configure LED pin %d\n", ret, led.pin);
//         return 1; // Indicate error
//     } else {
//         printk("Successfully configured LED pin %d\n", led.pin);
//     }
//
//     return ret;
// }
//
// int led_on_tick() {
//     int ret = gpio_pin_toggle_dt(&led);
//     printk("[LED] Toggled LED. Return code: %d\n", ret);
//     if (ret < 0) {
//         printk("Error %d: failed to toggle LED pin %d\n", ret, led.pin);
//     }
//     return ret;
// }
//
// // void button_on_key_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
// //     printk("[BTN] Button pressed at %" PRIu32 "\n", k_cycle_get_32());
// //     gpio_pin_toggle_dt(&led);
// // }
//
// // int button_initialize() {
// //     int ret;
// //
// //     if (!gpio_is_ready_dt(&button)) {
// //         printk("Error: button device %s is not ready\n",
// //                button.port->name);
// //         return 0;
// //     }
// //
// //     ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
// //     if (ret != 0) {
// //         printk("Error %d: failed to configure %s pin %d\n",
// //                ret, button.port->name, button.pin);
// //         return 0;
// //     }
// //
// //     ret = gpio_pin_interrupt_configure_dt(&button,
// //                                           GPIO_INT_EDGE_TO_ACTIVE);
// //     if (ret != 0) {
// //         printk("Error %d: failed to configure interrupt on %s pin %d\n",
// //                ret, button.port->name, button.pin);
// //         return 0;
// //     }
// //
// //     gpio_init_callback(&button_cb_data, button_on_key_pressed, BIT(button.pin));
// //     gpio_add_callback(button.port, &button_cb_data);
// //     printk("Set up button at %s pin %d\n", button.port->name, button.pin);
// //
// //     return ret;
// // }
//
// // int adc_initialize() {
// //     if (!adc_is_ready_dt(&adc_channel_idx0)) {
// //         printk("ADC controller device %s not ready\n", adc_channel_idx0.dev->name);
// //         return 0;
// //     }
// //
// //     int ret = adc_channel_setup_dt(&adc_channel_idx0);
// //     if (ret < 0) {
// //         printk("Could not setup channel #%d (%d)\n", 0, ret);
// //         return 0;
// //     }
// //
// //     return ret;
// // }
// //
// // int adc_on_tick() {
// //     printk("[ADC] %s, channel %d: ",
// //            adc_channel_idx0.dev->name,
// //            adc_channel_idx0.channel_id);
// //
// //     adc_sequence_init_dt(&adc_channel_idx0, &adc_sequence);
// //
// //     int ret = adc_read_dt(&adc_channel_idx0, &adc_sequence);
// //     if (ret < 0) {
// //         printk("Could not read (%d)\n", ret);
// //         return ret;
// //     }
// //
// //     printk("%"PRId32"\n", (int32_t) adc_sample);
// //
// //     return ret;
// // }
//
// int main() {
//     printk("Starting program on board: %s\n", CONFIG_BOARD);
//
//     if (led_initialize()!= 0) {
//         printk("LED init failed!\n");
//         return -1;
//     }
//     //button_initialize();
//     //adc_initialize();
//
//     printk("Entering blink loop...\n");
//     while (1) {
//         led_on_tick();
//         // LOG_INF("System started\n");
//         //adc_on_tick();
//         // printf("dupa\n");
//         // printk("Loop\n");
//         k_msleep(1000);
//     }
//     return 0;
// }
#include <errno.h>
#include <string.h>

#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/util.h>

#define STRIP_NODE		DT_ALIAS(led_strip)

#define SAMPLE_LED_UPDATE_DELAY 10

#define SAMPLE_LED_BRIGHTNESS 0xFF

#define STRIP_NUM_PIXELS	DT_PROP(DT_ALIAS(led_strip), chain_length)


#define DELAY_TIME K_MSEC(SAMPLE_LED_UPDATE_DELAY)

#define RGB(_r, _g, _b) { .r = (_r), .g = (_g), .b = (_b) }

static const struct led_rgb colors[] = {
    RGB(SAMPLE_LED_BRIGHTNESS, 0x00, 0x00), /* red */
    RGB(0x00, SAMPLE_LED_BRIGHTNESS, 0x00), /* green */
    RGB(0x00, 0x00, SAMPLE_LED_BRIGHTNESS), /* blue */
};

static struct led_rgb pixels[STRIP_NUM_PIXELS];

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);

int main(void)
{
    size_t color = 0;
    int rc;

    if (device_is_ready(strip)) {
        LOG_INF("Found LED strip device %s", strip->name);
    } else {
        LOG_ERR("LED strip device %s is not ready", strip->name);
        return 0;
    }

    LOG_INF("Displaying pattern on strip");
    while (1) {
        for (size_t cursor = 0; cursor < ARRAY_SIZE(pixels); cursor++) {
            memset(&pixels, 0x00, sizeof(pixels));
            memcpy(&pixels[cursor], &colors[color], sizeof(struct led_rgb));

            rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
            if (rc) {
                LOG_ERR("couldn't update strip: %d", rc);
            }

            k_sleep(DELAY_TIME);
        }

        color = (color + 1) % ARRAY_SIZE(colors);
    }

    return 0;
}