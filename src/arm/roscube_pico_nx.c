 /*
  * Author: Chih-Chieh Chang <chih-chieh.chang@adlinktech.com>
  * Copyright (c) 2021 ADLINK Technology Inc.
  * SPDX-License-Identifier: MIT
  */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/file.h>
#include <unistd.h>

#include "linux/gpio.h"
#include "mraa_internal.h"

#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <linux/i2c-dev.h>

#include "common.h"
#include "gpio.h"
#include "arm/roscube_pico_nx.h"
#include "gpio/gpio_chardev.h"

#define SYSFS_CLASS_GPIO "/sys/class/gpio"

#define PLATFORM_NAME "ROSCUBE-PICO-NX"

#define MRAA_ROSCUBE_GPIOCOUNT 5
#define MRAA_ROSCUBE_UARTCOUNT 1

#define MAX_SIZE 64
#define POLL_TIMEOUT
static volatile int base1,_fd;
static mraa_gpio_context gpio;
static char* uart_name[MRAA_ROSCUBE_UARTCOUNT] = {"COM1" };
static char* uart_path[MRAA_ROSCUBE_UARTCOUNT] = {"/dev/ttyTHS1"};
static unsigned char regIon[4]   = {0x33,0x36,0x3B,0x40};

static unsigned int  IonValue[4]   = {0xFF, 0xFF, 0xFF, 0xFF};

//pwm replace function for sm1509 
static int sx150x_pwm_init(int);

int _tperiod;

static mraa_result_t pwm_period_replace(mraa_pwm_context dev, int period)
{
	_tperiod = period;
	return MRAA_SUCCESS;
}

static float pwm_read_replace(mraa_pwm_context dev)
{
    unsigned char rx_tx_buf[3] = {0};

    if(dev->pin < 9)
    {
        if(_fd == -1)
        {
            return 0;
        }

        rx_tx_buf[0] = regIon[dev->pin-3];
        if(write(_fd, &(rx_tx_buf[0]), 1) == 1)
        {
            if(read(_fd, &(rx_tx_buf[1]), 1) == 1)
            {
                return (rx_tx_buf[1] / 2.55);
            }
        }
    }
    return 0;
}

static mraa_result_t pwm_write_replace(mraa_pwm_context dev, float duty)
{
    unsigned char rx_tx_buf[3] = {0};

	IonValue[dev->pin-3] = ((duty /_tperiod) * 255);
 
        if(dev->pin < 9)
        {
            if(_fd == -1)
            {
                return MRAA_ERROR_INVALID_RESOURCE;
            }

            rx_tx_buf[0] = regIon[dev->pin-3];
            rx_tx_buf[1] = IonValue[dev->pin-3];

            if(write(_fd, &(rx_tx_buf[0]), 2) != 2)
            {
                return MRAA_ERROR_NO_RESOURCES;
            }
            return MRAA_SUCCESS;
        }
        return MRAA_ERROR_NO_RESOURCES;
}


static mraa_result_t pwm_enable_replace(mraa_pwm_context dev, int enable)
{
    int pin = dev->pin;
    if(pin < 7 && pin > 2)
    {
        unsigned char rx_tx_buf[3] = {0};
        if (enable==0)
        {
            //If enable is 0, the EEPROM of the SX1509 will be reset to default.
            if(_fd != -1)
            {
                unsigned char rx_tx_buf_disable[3] = {0};
                if(_fd == -1)
                {
                    return MRAA_ERROR_NO_RESOURCES;
                }
                rx_tx_buf_disable[0] = 0x7D;
                rx_tx_buf_disable[1] = 0x12;
                if(write(_fd, &(rx_tx_buf_disable[0]), 2) != 2)
                {
                    return MRAA_ERROR_NO_RESOURCES;
                }
                rx_tx_buf_disable[1] = 0x34;
                if(write(_fd, &(rx_tx_buf_disable[0]), 2) != 2)
                {
                    return MRAA_ERROR_NO_RESOURCES;
                }
            }
            else
            {
                return MRAA_ERROR_NO_RESOURCES;
            }
                return MRAA_SUCCESS;
        }
        else
        {
            if(_fd != -1)
            {
                if(_fd == -1)
                {
                    return MRAA_ERROR_NO_RESOURCES;
                }
                rx_tx_buf[0] = regIon[pin-3];
                rx_tx_buf[1] = IonValue[pin-3];
                if(write(_fd, &(rx_tx_buf[0]), 2) != 2)
                {
                    return MRAA_ERROR_NO_RESOURCES;
                }
            }
            else
            {
                return MRAA_ERROR_NO_RESOURCES;
            }
                return MRAA_SUCCESS;
        }
    }
    return MRAA_ERROR_NO_RESOURCES;
}

static mraa_result_t pwm_init_raw_replace(mraa_pwm_context dev, int pin)
{
	char buffer[100] = {0};
	int i, fd;
	//syslog(LOG_WARNING, "pwm_init: pwm%i. chip info %d.", pin, dev->chipid);
	dev->advance_func->pwm_period_replace = pwm_period_replace;
	dev->advance_func->pwm_read_replace = pwm_read_replace;
	dev->advance_func->pwm_write_replace = pwm_write_replace;
	dev->advance_func->pwm_enable_replace = pwm_enable_replace;

	if(pin < 7 && pin > 2 )
	{
		if(sx150x_pwm_init(pin))
		{
			return MRAA_ERROR_NO_RESOURCES;
		}
		if((fd = open("/sys/class/gpio/unexport", O_WRONLY)) != -1)
		{
			//i = sprintf(buffer,"%d",base2 + pin);
			write(fd, buffer, i);
			close(fd);
		}
		if((fd = open("/sys/class/gpio/export", O_WRONLY)) != -1)
		{
			i = sprintf(buffer,"%d",base1 + pin);
			write(fd, buffer, i);
			close(fd);
			sprintf(buffer,"/sys/class/gpio/gpio%d/direction",base1 + pin);
			if((fd = open(buffer, O_WRONLY)) != -1)
			{
				write(fd, "out", 3);
				close(fd);
				sprintf(buffer,"/sys/class/gpio/gpio%d/value",base1 + pin);
				if((fd = open(buffer, O_WRONLY)) != -1)
				{
					write(fd, "0", 1);
					close(fd);
				}
			}
		}
		else
		{
			return MRAA_ERROR_NO_RESOURCES;
		}
		return MRAA_SUCCESS;
	}
	else
	{
		return MRAA_ERROR_NO_RESOURCES;
	}
	return MRAA_ERROR_NO_RESOURCES;
}
static int sx150x_init()
{
    char rx_tx_buf[100] = {0};
	int i, bus_num, fd;
    for(i = 0; i < 999; i++)
	{
		sprintf(rx_tx_buf,"/sys/class/gpio/gpiochip%d/device/name",i);
		if((fd = open(rx_tx_buf, O_RDONLY)) != -1)
		{
			int count = read(fd, rx_tx_buf, 7);
			if(count != 0)
			{
				if(strncmp(rx_tx_buf, "sx1509q", count) == 0)
				{
					base1 = i;
					break;
				}
			}
			close(fd);
		}
	}

	for(i = 0; i < 999;i++)
	{
		sprintf(rx_tx_buf,"/sys/bus/i2c/devices/%x-0070/name",i);
		if((fd = open(rx_tx_buf, O_RDONLY)) != -1)
		{
			int count = read(fd, rx_tx_buf, 7);
			if(count != 0)
			{
				if(strncmp(rx_tx_buf, "sx1509q", count) == 0)
				{
					bus_num = i;
					break;
				}
			}
			close(fd);
		}
	}

    sprintf(rx_tx_buf, "/dev/i2c-%d",bus_num);
    if((_fd = open(rx_tx_buf, O_RDWR)) < 0)
    {
        return -1;
    }

    if(ioctl(_fd, I2C_SLAVE_FORCE, 0x70) < 0)
    {
        return -1;
    }
// configuring clock and misc register for PWM feature
    rx_tx_buf[0] = 0x1E;
    if(write(_fd, &(rx_tx_buf[0]), 1) == 1)
    {
        if(read(_fd, &(rx_tx_buf[1]), 1) == 1)
        {
            rx_tx_buf[1] |= (1 << 6);
            rx_tx_buf[1] |= ~(1 << 5);
            if(write(_fd, &(rx_tx_buf[0]), 2) != 2)
            {
                return -1;
            }
        }
    }
    else
    {
            return -1;
    }

    rx_tx_buf[0] = 0x1F;
    if(write(_fd, &(rx_tx_buf[0]), 1) == 1)
    {
        if(read(_fd, &(rx_tx_buf[1]), 1) == 1)
        {
            rx_tx_buf[1] &= ~(1 << 7);
            rx_tx_buf[1] &= ~(1 << 3);
            rx_tx_buf[1] &= ~((0x7) << 4);
            rx_tx_buf[1] |= ((1 & 0x7) << 4);
            if(write(_fd, &(rx_tx_buf[0]), 2) == 2)
            {
                return 0;
            }
        }
    }

    return -1;
}

//configuring extra registers as pwm for extended gpio
static int sx150x_pwm_init(int pin)
{
    int i;
    int add = (pin < 8) ? 1 : 0;
    unsigned char rx_tx_buf[] = {0x0 + add, 0, 0x6 + add, 0, 0xE + add, 0, 0x20 + add, 0, 0x10 + add, 0};

    if(_fd == -1)
    {
        return -1;
    }
    for(i = 0; i < 9; i += 2)
    {
        if(write(_fd, &(rx_tx_buf[i]), 1) != 1)
        {
            return -1;
        }
        if(read(_fd, &(rx_tx_buf[i + 1]), 1) != 1)
        {
            return -1;
        }
        if((i == 2) || (i == 6))
            rx_tx_buf[i+1] |= (1 << (pin % 8));
        else
            rx_tx_buf[i+1]  &= ~(1 << (pin % 8));
        if(write(_fd, &(rx_tx_buf[i]), 2) != 2)
        {
            return -1;
        }
    }
    return 0;
}

// utility function to setup pin mapping of boards
static mraa_result_t mraa_roscube_set_pininfo(mraa_board_t* board, int mraa_index, char* name,
                                              mraa_pincapabilities_t caps, int sysfs_pin)
{
    if (mraa_index < board->phy_pin_count) 
    {
        mraa_pininfo_t* pin_info = &board->pins[mraa_index];
        strncpy(pin_info->name, name, MRAA_PIN_NAME_SIZE);
        pin_info->capabilities = caps;
        if (caps.gpio) 
        {
            pin_info->gpio.pinmap = sysfs_pin;
            pin_info->gpio.mux_total = 0;
        }
        if (caps.i2c) 
        {
            pin_info->i2c.pinmap = 1;
            pin_info->i2c.mux_total = 0;
        }
        if (caps.uart) 
        {
            pin_info->uart.mux_total = 0;
        }
        if (caps.spi) 
        {
            pin_info->spi.mux_total = 0;
        }
        if (caps.pwm) 
        {
            pin_info->pwm.mux_total = 0;
            pin_info->pwm.parent_id = 0;
            pin_info->pwm.pinmap = sysfs_pin - base1;
        }
        return MRAA_SUCCESS;
    }
    return MRAA_ERROR_INVALID_RESOURCE;
}

static mraa_result_t mraa_roscube_get_pin_index(mraa_board_t* board, char* name, int* pin_index)
{
    for (int i = 0; i < board->phy_pin_count; ++i) 
    {
        if (strncmp(name, board->pins[i].name, MRAA_PIN_NAME_SIZE) == 0) 
        {
            *pin_index = i;
            return MRAA_SUCCESS;
        }
    }

    syslog(LOG_CRIT, "ROSCUBE PICO NX: Failed to find pin name %s", name);

    return MRAA_ERROR_INVALID_RESOURCE;
}

static mraa_result_t mraa_roscube_init_uart(mraa_board_t* board, int index)
{
    if (index >= MRAA_ROSCUBE_UARTCOUNT)
        return MRAA_ERROR_INVALID_RESOURCE;
    board->uart_dev[index].index = index;
    board->uart_dev[index].device_path = uart_path[index];
    board->uart_dev[index].name = uart_name[index];
    board->uart_dev[index].tx = -1;
    board->uart_dev[index].rx = -1;
    board->uart_dev[index].cts = -1;
    board->uart_dev[index].rts = -1;
    return MRAA_SUCCESS;
}

mraa_board_t* mraa_roscube_pico_nx()
{
    int i, fd, i2c_bus_num;
    char buffer[60] = {0}, *line = NULL;
    FILE *fh;
    size_t len;

    mraa_board_t* b = (mraa_board_t*) calloc(1, sizeof (mraa_board_t));

    if (b == NULL) 
    {
        return NULL;
    }
    b->platform_name = PLATFORM_NAME;
    b->phy_pin_count = MRAA_ROSCUBE_PICO_NX_PINCOUNT;
    b->gpio_count = MRAA_ROSCUBE_GPIOCOUNT;
    b->chardev_capable = 0;

    b->pins = (mraa_pininfo_t*) malloc(sizeof(mraa_pininfo_t) * MRAA_ROSCUBE_PICO_NX_PINCOUNT);
    if (b->pins == NULL) 
    {
        goto error;
    }

    b->adv_func = (mraa_adv_func_t *) calloc(1, sizeof (mraa_adv_func_t));
    if (b->adv_func == NULL) 
    {
        free(b->pins);
        goto error;
    }

    b->adv_func->gpio_isr_replace = NULL;
    b->adv_func->gpio_close_pre = NULL;
    b->adv_func->gpio_init_pre = NULL;

    // We fix the base currently.
    base1 = 231;

    syslog(LOG_NOTICE, "ROSCube Pico NX: base1 %d \n", base1);

    // Configure PWM
    b->pwm_dev_count = 4;
    b->pwm_default_period = 5000;
    b->pwm_max_period = 2147483;
    b->pwm_min_period = 1;

    // initializations of pwm functions
    b->adv_func->pwm_init_raw_replace = pwm_init_raw_replace;
    b->adv_func->pwm_period_replace = pwm_period_replace;
    b->adv_func->pwm_read_replace = pwm_read_replace;
    b->adv_func->pwm_write_replace = pwm_write_replace;
    b->adv_func->pwm_enable_replace = pwm_enable_replace;

    mraa_roscube_set_pininfo(b, 1,  "5V",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 2,  "GND",              (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 3,  "FORCED_RECOVERY",  (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 4,  "GND",              (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 5,  "SYS_RST",          (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 6,  "GND",              (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 7,  "DEBUG_CONSOLE_TX", (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 8,  "DEBUG_CONSOLE_RX", (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 9,  "GND",              (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 10, "I2C0_SCL",         (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 1, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 11, "I2C0_SDA",         (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 1, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 12, "SPI0_CS1",         (mraa_pincapabilities_t){ 1, 0, 0, 0, 1, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 13, "SPI0_CS0",         (mraa_pincapabilities_t){ 1, 0, 0, 0, 1, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 14, "GND",              (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 15, "UART0_RX",         (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 1 }, -1);
    mraa_roscube_set_pininfo(b, 16, "UART0_TX",         (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 1 }, -1);
    mraa_roscube_set_pininfo(b, 17, "GND",              (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 18, "V5",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 19, "V5",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 20, "GND",              (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 21, "CAN_H",            (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 22, "CAN_L",            (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);    
    mraa_roscube_set_pininfo(b, 23, "GND",              (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 24, "GPIO7",            (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1+7);
    mraa_roscube_set_pininfo(b, 25, "GPIO6_PWM",        (mraa_pincapabilities_t){ 1, 1, 1, 0, 0, 0, 0, 0 }, base1+6);
    mraa_roscube_set_pininfo(b, 26, "GPIO5_PWM",        (mraa_pincapabilities_t){ 1, 1, 1, 0, 0, 0, 0, 0 }, base1+5);
    mraa_roscube_set_pininfo(b, 27, "GPIO4_PWM",        (mraa_pincapabilities_t){ 1, 1, 1, 0, 0, 0, 0, 0 }, base1+4);
    mraa_roscube_set_pininfo(b, 28, "GPIO3_PWM",        (mraa_pincapabilities_t){ 1, 1, 1, 0, 0, 0, 0, 0 }, base1+3);
    mraa_roscube_set_pininfo(b, 29, "GND",              (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 30, "SPI_CLK",          (mraa_pincapabilities_t){ 1, 0, 0, 0, 1, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 31, "MISO0",            (mraa_pincapabilities_t){ 1, 0, 0, 0, 1, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 32, "MOSI0",            (mraa_pincapabilities_t){ 1, 0, 0, 0, 1, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 33, "3.3V",             (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 34, "GND",              (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 35, "I2C1_SCL",         (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 1, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 36, "I2C1_SDA",         (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 1, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 37, "3.3V",             (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
   
    b->uart_dev_count = MRAA_ROSCUBE_UARTCOUNT;
    for (int i = 0; i < MRAA_ROSCUBE_UARTCOUNT; i++)
        mraa_roscube_init_uart(b, i);
    b->def_uart_dev = 0;

    // Configure SPI
    b->spi_bus_count = 0;
    b->spi_bus[b->spi_bus_count].bus_id = 0;
    b->spi_bus[b->spi_bus_count].slave_s = 0;
    mraa_roscube_get_pin_index(b, "SPI0_CS0",  &(b->spi_bus[b->spi_bus_count].cs));
    mraa_roscube_get_pin_index(b, "MOSI0", &(b->spi_bus[b->spi_bus_count].mosi));
    mraa_roscube_get_pin_index(b, "MISO0", &(b->spi_bus[b->spi_bus_count].miso));
    mraa_roscube_get_pin_index(b, "SPI_CLK",  &(b->spi_bus[b->spi_bus_count].sclk));
    b->spi_bus_count++;

    b->spi_bus[b->spi_bus_count].bus_id = 0;
    b->spi_bus[b->spi_bus_count].slave_s = 1;
    mraa_roscube_get_pin_index(b, "SPI0_CS1",  &(b->spi_bus[b->spi_bus_count].cs));
    mraa_roscube_get_pin_index(b, "MOSI0", &(b->spi_bus[b->spi_bus_count].mosi));
    mraa_roscube_get_pin_index(b, "MISO0", &(b->spi_bus[b->spi_bus_count].miso));
    mraa_roscube_get_pin_index(b, "SPI_CLK",  &(b->spi_bus[b->spi_bus_count].sclk));
    b->spi_bus_count++;

    // Set number of i2c adaptors usable from userspace
    b->i2c_bus_count =0;
    b->def_i2c_bus = 1;

    i2c_bus_num = mraa_find_i2c_bus("c240000.i2c",0);
    if (i2c_bus_num != -1) 
    {
        b->i2c_bus[0].bus_id = i2c_bus_num;
        mraa_roscube_get_pin_index(b, "I2C0_SDA", (int*) &(b->i2c_bus[0].sda));
        mraa_roscube_get_pin_index(b, "I2C0_SCL", (int*) &(b->i2c_bus[0].scl));
        b->i2c_bus_count++;
    }

    i2c_bus_num = mraa_find_i2c_bus("31e0000.i2c",0);
    if (i2c_bus_num != -1) 
    {
        b->i2c_bus[1].bus_id = i2c_bus_num;
        mraa_roscube_get_pin_index(b, "I2C1_SDA", (int*) &(b->i2c_bus[1].sda));
        mraa_roscube_get_pin_index(b, "I2C1_SCL", (int*) &(b->i2c_bus[1].scl));
        b->i2c_bus_count++;
    }
    if(sx150x_init() < 0)
    {
	    _fd = -1;
    }
#if 0 
    const char* pinctrl_path = "/sys/bus/platform/drivers/broxton-pinctrl";
    int have_pinctrl = access(pinctrl_path, F_OK) != -1;
    syslog(LOG_NOTICE, "ROSCUBE X: kernel pinctrl driver %savailable", have_pinctrl ? "" : "un");
#endif

    return b;

error:
    syslog(LOG_CRIT, "ROSCUBE PICO NX: Platform failed to initialise");
    free(b);
    close(_fd);
    return NULL;
}
