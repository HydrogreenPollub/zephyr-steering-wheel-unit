
#include <errno.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/util.h>

#include "steering_wheel.h"

LOG_MODULE_REGISTER(main);

int main(void)
{
    steering_wheel_init();
    return 0;
}