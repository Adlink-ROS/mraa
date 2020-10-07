 /*
 * Author: Dan O'Donovan <dan@emutex.com>
 * Author: Santhanakumar A <santhanakumar.a@adlinktech.com>

 * Copyright (c) 2019 ADLINK Technology Inc.
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
#include "arm/roscube_x.h"
#include "gpio/gpio_chardev.h"

#define SYSFS_CLASS_GPIO "/sys/class/gpio"

#define PLATFORM_NAME "ROSCUBE-X"

#define MRAA_ROSCUBE_GPIOCOUNT 20
#define MRAA_ROSCUBE_UARTCOUNT 3
#define MRAA_ROSCUBE_X_LEDCOUNT 6
#define MAX_SIZE 64
#define POLL_TIMEOUT
const char* ROSCube_X_LED[MRAA_ROSCUBE_X_LEDCOUNT] = { "LED1", "LED2", "LED3","LED4", "LED5", "LED6"};
static volatile int base1, base2, _fd;
static mraa_gpio_context gpio;
static char* uart_name[MRAA_ROSCUBE_UARTCOUNT] = {"COM1", "COM2","COM3_DB50" };
static char* uart_path[MRAA_ROSCUBE_UARTCOUNT] = {"/dev/ttyUSB0", "/dev/ttyUSB1","/dev/ttyTHS0"};

// utility function to setup pin mapping of boards
static mraa_result_t mraa_roscube_set_pininfo(mraa_board_t* board, int mraa_index, char* name,
                                              mraa_pincapabilities_t caps, int sysfs_pin)
{
    if (mraa_index < board->phy_pin_count) {
        mraa_pininfo_t* pin_info = &board->pins[mraa_index];
        strncpy(pin_info->name, name, MRAA_PIN_NAME_SIZE);
        pin_info->capabilities = caps;
        if (caps.gpio) {
            pin_info->gpio.pinmap = sysfs_pin;
            pin_info->gpio.mux_total = 0;
        }
        if (caps.i2c) {
            pin_info->i2c.pinmap = 1;
            pin_info->i2c.mux_total = 0;
        }
        if (caps.uart) {
            pin_info->uart.mux_total = 0;
        }
        if (caps.spi) {
            pin_info->spi.mux_total = 0;
        }
        return MRAA_SUCCESS;
    }
    return MRAA_ERROR_INVALID_RESOURCE;
}

static mraa_result_t mraa_roscube_get_pin_index(mraa_board_t* board, char* name, int* pin_index)
{
    for (int i = 0; i < board->phy_pin_count; ++i) {
        if (strncmp(name, board->pins[i].name, MRAA_PIN_NAME_SIZE) == 0) {
            *pin_index = i;
            return MRAA_SUCCESS;
        }
    }

    syslog(LOG_CRIT, "ROSCUBE X: Failed to find pin name %s", name);

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

mraa_board_t* mraa_roscube_x()
{
    int i, fd, i2c_bus_num;
    char buffer[60] = {0}, *line = NULL;
    FILE *fh;
    size_t len;

    mraa_board_t* b = (mraa_board_t*) calloc(1, sizeof (mraa_board_t));

    if (b == NULL) {
        return NULL;
    }

    b->led_dev[0].name = (char*) ROSCube_X_LED[0];
    b->led_dev[1].name = (char*) ROSCube_X_LED[1];
    b->led_dev[2].name = (char*) ROSCube_X_LED[2];
    b->led_dev[3].name = (char*) ROSCube_X_LED[3];
    b->led_dev[4].name = (char*) ROSCube_X_LED[4];
    b->led_dev[5].name = (char*) ROSCube_X_LED[5];
    b->led_dev_count = MRAA_ROSCUBE_X_LEDCOUNT;
    b->platform_name = PLATFORM_NAME;
    b->phy_pin_count = MRAA_ROSCUBE_X_PINCOUNT;
    b->gpio_count = MRAA_ROSCUBE_GPIOCOUNT;
    b->chardev_capable = 0;

    b->pins = (mraa_pininfo_t*) malloc(sizeof(mraa_pininfo_t) * MRAA_ROSCUBE_X_PINCOUNT);
    if (b->pins == NULL) {
        goto error;
    }

    b->adv_func = (mraa_adv_func_t *) calloc(1, sizeof (mraa_adv_func_t));
    if (b->adv_func == NULL) {
        free(b->pins);
        goto error;
    }

    b->adv_func->gpio_isr_replace = NULL;
    b->adv_func->gpio_close_pre = NULL;
    b->adv_func->gpio_init_pre = NULL;

#if 0 // TODO: PWM
    // initializations of pwm functions
    b->adv_func->pwm_init_raw_replace = pwm_init_raw_replace;
    b->adv_func->pwm_period_replace = pwm_period_replace;
    b->adv_func->pwm_read_replace = pwm_read_replace;
    b->adv_func->pwm_write_replace = pwm_write_replace;
    b->adv_func->pwm_enable_replace = pwm_enable_replace;
#endif

#if 0
    for(i = 0; i < 999; i++) {
        sprintf(buffer,"/sys/class/gpio/gpiochip%d/device/name",i);
        if((fd = open(buffer, O_RDONLY)) != -1) {
            int count = read(fd,buffer,7);
            if(count != 0) {
                // GPIO 16-19
                if(strncmp(buffer, "pca9534", count) == 0) {
                    base2 = i;
                }
                // GPIO 0-15
                if(strncmp(buffer, "pca9535", count) == 0) {
                    base1 = i;
                }
            }
            close(fd);
        }
    }
#else
    // We fix the base currently.
    base1 = 216;
#endif

    syslog(LOG_NOTICE, "ROSCubeX: base1 %d base2 %d\n", base1, base2);

// TODO: check PWM
#if 0
    // Configure PWM
    b->pwm_dev_count = 0;
    b->pwm_default_period = 5000;
    b->pwm_max_period = 218453;
    b->pwm_min_period = 1;
#endif

    mraa_roscube_set_pininfo(b, 1,  "ADC1_isolation", (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 2,  "ADC2_isolation", (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 3,  "Unused",         (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 4,  "Unused",         (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 5,  "GPIO0",          (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 0);
    mraa_roscube_set_pininfo(b, 6,  "GPIO1",          (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 1);
    mraa_roscube_set_pininfo(b, 7,  "GPIO2",          (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 2);
    mraa_roscube_set_pininfo(b, 8,  "GPIO3",          (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 3);
    mraa_roscube_set_pininfo(b, 9,  "GPIO4",          (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 4);
    mraa_roscube_set_pininfo(b, 10, "GPIO5",          (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 5);
    mraa_roscube_set_pininfo(b, 11, "GPIO6",          (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 6);
    mraa_roscube_set_pininfo(b, 12, "GPIO7",          (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 7);
    mraa_roscube_set_pininfo(b, 13, "GPIO8",          (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 8);
    mraa_roscube_set_pininfo(b, 14, "GPIO9",          (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 9);
    mraa_roscube_set_pininfo(b, 15, "GPIO10",         (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 10);
    mraa_roscube_set_pininfo(b, 16, "GPIO11",         (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 11);
    mraa_roscube_set_pininfo(b, 17, "GPIO12",         (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 12);
    mraa_roscube_set_pininfo(b, 18, "GND",            (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 19, "Unused",         (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 20, "Unused",         (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 21, "Unused",         (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 22, "PWM",            (mraa_pincapabilities_t){ 1, 0, 1, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 23, "SPI_CLK",        (mraa_pincapabilities_t){ 1, 0, 0, 0, 1, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 24, "SPI_CS",         (mraa_pincapabilities_t){ 1, 0, 0, 0, 1, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 25, "SPI_MISO",       (mraa_pincapabilities_t){ 1, 0, 0, 0, 1, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 26, "SPI_MOSI",       (mraa_pincapabilities_t){ 1, 0, 0, 0, 1, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 27, "GPIO13",         (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 13);
    mraa_roscube_set_pininfo(b, 28, "GPIO14",         (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 14);
    mraa_roscube_set_pininfo(b, 29, "GPIO15",         (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 15);
    mraa_roscube_set_pininfo(b, 30, "GPIO16",         (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 17);
    mraa_roscube_set_pininfo(b, 31, "GPIO17",         (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 18);
    mraa_roscube_set_pininfo(b, 32, "GPIO18",         (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 19);
    mraa_roscube_set_pininfo(b, 33, "GPIO19",         (mraa_pincapabilities_t){ 1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 20);
    mraa_roscube_set_pininfo(b, 34, "GND",            (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 35, "Unused",         (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 36, "Unused",         (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 37, "Unused",         (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 38, "UART_RX",        (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 1 }, -1);
    mraa_roscube_set_pininfo(b, 39, "UART_TX",        (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 0, 0, 1 }, -1);
    mraa_roscube_set_pininfo(b, 40, "CAN_H",          (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 41, "CAN_L",          (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 42, "I2C_CLK",        (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 1, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 43, "I2C_DATA",       (mraa_pincapabilities_t){ 1, 0, 0, 0, 0, 1, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 45, "5V",             (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 46, "3.3V",           (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 47, "GND",            (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 48, "GND",            (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 49, "GND",            (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 50, "GND",            (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
   
    b->uart_dev_count = MRAA_ROSCUBE_UARTCOUNT;
    for (int i = 0; i < MRAA_ROSCUBE_UARTCOUNT; i++)
        mraa_roscube_init_uart(b, i);
    b->def_uart_dev = 0;

#if 0 // TODO: SPI
    // Configure SPI #0 CS1
    b->spi_bus_count = 0;
    b->spi_bus[b->spi_bus_count].bus_id = 1;
    b->spi_bus[b->spi_bus_count].slave_s = 0;
    mraa_roscube_get_pin_index(b, "SPI_0_CE0",  &(b->spi_bus[b->spi_bus_count].cs));
    mraa_roscube_get_pin_index(b, "SPI_0_MOSI", &(b->spi_bus[b->spi_bus_count].mosi));
    mraa_roscube_get_pin_index(b, "SPI_0_MISO", &(b->spi_bus[b->spi_bus_count].miso));
    mraa_roscube_get_pin_index(b, "SPI_0_SCLK",  &(b->spi_bus[b->spi_bus_count].sclk));
    b->spi_bus_count++;

    b->spi_bus[b->spi_bus_count].bus_id = 1;
    b->spi_bus[b->spi_bus_count].slave_s = 1;
    mraa_roscube_get_pin_index(b, "SPI_0_CE1",  &(b->spi_bus[b->spi_bus_count].cs));
    mraa_roscube_get_pin_index(b, "SPI_0_MOSI", &(b->spi_bus[b->spi_bus_count].mosi));
    mraa_roscube_get_pin_index(b, "SPI_0_MISO", &(b->spi_bus[b->spi_bus_count].miso));
    mraa_roscube_get_pin_index(b, "SPI_0_SCLK",  &(b->spi_bus[b->spi_bus_count].sclk));
    b->spi_bus_count++;
#endif
    // Set number of i2c adaptors usable from userspace
    b->i2c_bus_count = 0;
    b->def_i2c_bus = 0;

    i2c_bus_num = mraa_find_i2c_bus("31e0000.i2c",0);
    if (i2c_bus_num != -1) {
        b->i2c_bus[0].bus_id = i2c_bus_num;
        mraa_roscube_get_pin_index(b, "I2C_CLK", (int*) &(b->i2c_bus[0].sda));
        mraa_roscube_get_pin_index(b, "I2C_DATA", (int*) &(b->i2c_bus[0].scl));
        b->i2c_bus_count++;
    }

#if 0 
    const char* pinctrl_path = "/sys/bus/platform/drivers/broxton-pinctrl";
    int have_pinctrl = access(pinctrl_path, F_OK) != -1;
    syslog(LOG_NOTICE, "ROSCUBE X: kernel pinctrl driver %savailable", have_pinctrl ? "" : "un");
#endif

    return b;

error:
    syslog(LOG_CRIT, "ROSCUBE X: Platform failed to initialise");
    free(b);
    close(_fd);
    return NULL;
}
