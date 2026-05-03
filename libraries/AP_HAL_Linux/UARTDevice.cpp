#include "UARTDevice.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <asm/ioctls.h>
#include <asm/termbits.h>
#include <unistd.h>

#include <AP_HAL/AP_HAL.h>


#define SBUS_NUM_CHANNELS 16
#define SBUS_CH17_MASK        0x01
#define SBUS_CH18_MASK        0x02
#define SBUS_LOST_FRAME_MASK  0x04
#define SBUS_FAILSAFE_MASK    0x08

typedef struct {
      uint8_t lost_frame;
      uint8_t failsafe;
      uint8_t ch17, ch18;
      uint16_t channels[SBUS_NUM_CHANNELS];
    }  SbusData;

#ifndef LINUX_UART_DEBUG
#define LINUX_UART_DEBUG 0
#endif

#if LINUX_UART_DEBUG
#define debug(fmt, args ...)  do {printf("%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ## args); } while(0)
#else
#define debug(fmt, args ...)
#endif 


void SbusRx_Parse(const uint8_t *buf_,  SbusData *data_) {
	/* Grab the channel data */
	data_->channels[0]  = (uint16_t)(buf_[1] |
										((buf_[2] << 8) & 0x07FF));
	data_->channels[1]  = (uint16_t)((buf_[2] >> 3) |
										((buf_[3] << 5) & 0x07FF));
	data_->channels[2]  = (uint16_t)((buf_[3] >> 6) |
										(buf_[4] << 2) |
										((buf_[5] << 10) & 0x07FF));
	data_->channels[3]  = (uint16_t)((buf_[5] >> 1) |
										((buf_[6] << 7) & 0x07FF));
	data_->channels[4]  = (uint16_t)((buf_[6] >> 4) |
										((buf_[7] << 4) & 0x07FF));
	data_->channels[5]  = (uint16_t)((buf_[7] >> 7) |
										(buf_[8] << 1) |
										((buf_[9] << 9) & 0x07FF));
	data_->channels[6]  = (uint16_t)((buf_[9] >> 2) |
										((buf_[10] << 6) & 0x07FF));
	data_->channels[7]  = (uint16_t)((buf_[10] >> 5) |
										((buf_[11] << 3) & 0x07FF));
	data_->channels[8]  = (uint16_t)(buf_[12] |
										((buf_[13] << 8) & 0x07FF));
	data_->channels[9]  = (uint16_t)((buf_[13] >> 3) |
										((buf_[14] << 5) & 0x07FF));
	data_->channels[10] = (uint16_t)((buf_[14] >> 6) |
										(buf_[15] << 2) |
										((buf_[16] << 10) & 0x07FF));
	data_->channels[11] = (uint16_t)((buf_[16] >> 1) |
										((buf_[17] << 7) & 0x07FF));
	data_->channels[12] = (uint16_t)((buf_[17] >> 4) |
										((buf_[18] << 4) & 0x07FF));
	data_->channels[13] = (uint16_t)((buf_[18] >> 7) |
										(buf_[19] << 1) |
										((buf_[20] << 9) & 0x07FF));
	data_->channels[14] = (uint16_t)((buf_[20] >> 2) |
										((buf_[21] << 6) & 0x07FF));
	data_->channels[15] = (uint16_t)((buf_[21] >> 5) |
										((buf_[22] << 3) & 0x07FF));
	/* CH 17 */
	data_->ch17 = buf_[23] & SBUS_CH17_MASK;
	/* CH 18 */
	data_->ch18 = buf_[23] & SBUS_CH18_MASK;
	/* Grab the lost frame */
	data_->lost_frame = buf_[23] & SBUS_LOST_FRAME_MASK;
	/* Grab the failsafe */
	data_->failsafe = buf_[23] & SBUS_FAILSAFE_MASK;
  return;
}



UARTDevice::UARTDevice(const char *device_path):
    _device_path(device_path)
{
}

UARTDevice::~UARTDevice()
{
}

bool UARTDevice::close()
{
    if (_fd != -1) {
        if (::close(_fd) < 0) {
            return false;
        }
    }

    _fd = -1;

    return true;
}

bool UARTDevice::open()
{
    _fd = ::open(_device_path, O_RDWR | O_CLOEXEC | O_NOCTTY);

    if (_fd < 0) {
        ::fprintf(stderr, "Failed to open UART device %s - %s\n",
                  _device_path, strerror(errno));
        return false;
    }

    _disable_crlf();

    return true;
}

ssize_t UARTDevice::read(uint8_t *buf, uint16_t n)
{
    return ::read(_fd, buf, n);
}

ssize_t UARTDevice::write(const uint8_t *buf, uint16_t n)
{
    struct pollfd fds;
    fds.fd = _fd;
    fds.events = POLLOUT;
    fds.revents = 0;

    int ret = 0;

    if (poll(&fds, 1, 0) == 1) {
        ret = ::write(_fd, buf, n);
    }

// SBUS parce
if(n==25) {
	SbusData sbus_data;
	SbusRx_Parse(buf, &sbus_data);
	debug("ch:%d,%d failsafe:%d, ch17:%d, ch18:%d, start:%d end:%d\n", sbus_data.channels[0], sbus_data.channels[1], sbus_data.failsafe,sbus_data.ch17,sbus_data.ch18,buf[0],buf[24]);
}


    return ret;
}

void UARTDevice::set_blocking(bool blocking)
{
    int flags = fcntl(_fd, F_GETFL, 0);

    if (blocking) {
        flags = flags & ~O_NONBLOCK;
    } else {
        flags = flags | O_NONBLOCK;
    }

    if (fcntl(_fd, F_SETFL, flags) < 0) {
        ::fprintf(stderr, "Failed to make UART nonblocking %s - %s\n",
                  _device_path, strerror(errno));
    }

}

void UARTDevice::_disable_crlf()
{
    struct termios2 t = { 0 };

    if (ioctl(_fd, TCGETS2, &t) != 0) {
        ::fprintf(stderr, "Failed to read serial options for %s - %s\n",
                  _device_path, strerror(errno));
        return;
    }

    // disable LF -> CR/LF
    t.c_iflag &= ~(BRKINT | ICRNL | IMAXBEL | IXON | IXOFF);
    t.c_oflag &= ~(OPOST | ONLCR);
    t.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE);
    t.c_cc[VMIN] = 0;

    if (ioctl(_fd, TCSETS2, &t) != 0) {
        ::fprintf(stderr, "Failed to disable crlf on %s - %s\n",
                  _device_path, strerror(errno));
        return;
    }
}

void UARTDevice::set_speed(uint32_t baudrate)
{
    struct termios2 tio = { 0 };

	debug("UARTDevice::set_speed: %s baudrate: %d\n", _device_path, baudrate);


    if (ioctl(_fd, TCGETS2, &tio) != 0) {
        ::fprintf(stderr, "Failed to read serial options for %s - %s\n",
                  _device_path, strerror(errno));
        return;
    }

    // use CBAUDEX and B(aud)OTHER to gain access to "non-standard" rates that are common for eg. RC receivers
    tio.c_cflag &= ~CBAUD;
    tio.c_cflag |= CBAUDEX;
#if defined BOTHER    
    tio.c_cflag |= BOTHER;
#endif
    tio.c_ispeed = baudrate;
    tio.c_ospeed = baudrate;

    if (ioctl(_fd, TCSETS2, &tio) != 0) {
        ::fprintf(stderr, "Failed to set serial baud to %d for %s - %s\n",
                  baudrate, _device_path, strerror(errno));
        return;
    }
}

void UARTDevice::set_flow_control(AP_HAL::UARTDriver::flow_control flow_control_setting)
{

	debug("UARTDevice::set_flow_control:%s\n", _device_path);

    if (_flow_control == flow_control_setting) {
        return;
    }

    struct termios2 t = { 0 };

    if (ioctl(_fd, TCGETS2, &t) != 0) {
        ::fprintf(stderr, "Failed to read serial options for %s - %s\n",
                  _device_path, strerror(errno));
        return;
    }

    if (flow_control_setting != AP_HAL::UARTDriver::FLOW_CONTROL_DISABLE) {
        t.c_cflag |= CRTSCTS;
    } else {
        t.c_cflag &= ~CRTSCTS;
    }

    if (ioctl(_fd, TCSETS2, &t) != 0) {
        ::fprintf(stderr, "Failed to set flow control for %s - %s\n",
                  _device_path, strerror(errno));
        return;
    }

    _flow_control = flow_control_setting;
}

void UARTDevice::set_parity(int v)
{
    struct termios2 t = { 0 };
	debug("UARTDevice::set_parity:%s %d\n",_device_path, v);

    if (ioctl(_fd, TCGETS2, &t) != 0) {
        ::fprintf(stderr, "Failed to read serial options for %s - %s\n",
                  _device_path, strerror(errno));
        return;
    }

    if (v != 0) {
        // enable parity
        t.c_cflag |= PARENB;
        if (v == 1) {
            t.c_cflag |= PARODD;
        } else {
            t.c_cflag &= ~PARODD;
        }
    }
    else {
        // disable parity
        t.c_cflag &= ~PARENB;
    }

    if (ioctl(_fd, TCSETS2, &t) != 0) {
        ::fprintf(stderr, "Failed to set parity for %s - %s\n",
                  _device_path, strerror(errno));
        return;
    }
}

void UARTDevice::set_stop_bits(int n)
{
    struct termios2 t = { 0 };

	debug("UARTDevice::set_stop_bits: %s-%d\n",_device_path, n);

    if (ioctl(_fd, TCGETS2, &t) != 0) {
        ::fprintf(stderr, "Failed to read serial options for %s - %s\n",
                  _device_path, strerror(errno));
        return;
    }

    if (n == 2) {
        t.c_cflag |= CSTOPB; // Set two stop bits

    } else {
        t.c_cflag &= ~CSTOPB; // Clear the flag to use one stop bit
    }

    if (ioctl(_fd, TCSETS2, &t) != 0) {
        ::fprintf(stderr, "Failed to set parity for %s - %s\n",
                  _device_path, strerror(errno));
        return;
    }
}
