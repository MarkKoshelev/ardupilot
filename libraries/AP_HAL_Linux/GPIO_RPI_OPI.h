#pragma once

#include <stdint.h>
#include "GPIO_RPI_HAL.h"

namespace Linux {

/**
 * @brief Class for Orange PI GPIO control
 *
 */

class GPIO_RPI_OPI : public GPIO_RPI_HAL {
public:
    GPIO_RPI_OPI();
    void    init() override;
    void    pinMode(uint8_t pin, uint8_t output) override;
    void    pinMode(uint8_t pin, uint8_t output, uint8_t alt) override;
    uint8_t read(uint8_t pin) override;
    void    write(uint8_t pin, uint8_t value) override;
    void    toggle(uint8_t pin) override;

    /* Alternative interface: */
    AP_HAL::DigitalSource* channel(uint16_t n);

    /* return true if USB cable is connected */
    bool    usb_connected(void);

private:
	uint32_t _gpio_output_port_status = 0;
};

}
