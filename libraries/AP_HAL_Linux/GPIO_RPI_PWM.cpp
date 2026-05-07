#include <AP_HAL/AP_HAL_Boards.h>

#include "GPIO_RPI_PWM.h"

#if HAL_LINUX_GPIO_OBAL_ENABLED

const unsigned Linux::GPIO_Sysfs::pin_table[] = {
    [GPIO_RPI_PWM1] =    500,
    [GPIO_RPI_PWM2] =    501,
};

const uint8_t Linux::GPIO_Sysfs::n_pins = _GPIO_RPI_PWM_MAX;

static_assert(ARRAY_SIZE(Linux::GPIO_Sysfs::pin_table) == _GPIO_RPI_PWM_MAX,
              "GPIO pin_table must have the same size of entries in enum");

#endif  // HAL_LINUX_GPIO_OBAL_ENABLED
