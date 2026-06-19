
#include <errno.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/util.h>

#include "steering_wheel_inputs.h"
#include "steering_wheel_display.h"
#include "steering_wheel_can.h"

LOG_MODULE_REGISTER(main);

int main(void)
{
    swu_display_init();
    swu_can_init();
    return 0;
}
