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
#include "x86/roscube_i.h"
#include "gpio/gpio_chardev.h"
#include "mraa/gpio.h"

#define SYSFS_CLASS_GPIO "/sys/class/gpio"
#define PLATFORM_NAME "ROSCUBE-I"

#define MRAA_ROSCUBE_GPIOCOUNT 16
#define MRAA_ROSCUBE_UARTCOUNT 2
#define MRAA_ROSCUBE_LEDCOUNT 6

#define MAX_SIZE 64
#define POLL_TIMEOUT

static volatile int base1, base2, _fd;
#define base1  220
#define base2 253
#define led_base 276
static mraa_gpio_context gpio;
static char* uart_name[MRAA_ROSCUBE_UARTCOUNT] = {"COM1", "COM2"};
static char* uart_path[MRAA_ROSCUBE_UARTCOUNT] = {"/dev/ttyS0", "/dev/ttyS1"};

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

    syslog(LOG_CRIT, "ROSCUBE I: Failed to find pin name %s", name);

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

int index_mapping(int index)
{
    return index + led_base;
}

mraa_result_t rqi_led_init(int index)
{
    char  export_path[64];
    char export_val[64];
    char led_path_dir[64];
    char led_path_val[64];
    char led_setup_val[64];
    char buf[64];

    index = index_mapping(index);
    snprintf(export_path,64,SYSFS_CLASS_GPIO "/export");
    int export_file = open(export_path, O_WRONLY);
    if (export_file == -1) {
        syslog(LOG_CRIT, "ROSCUBE I: rqi_led_init unable to open GPIO export (errno %d)", errno);
        return MRAA_ERROR_INVALID_RESOURCE;
    }
    int length_export = snprintf(export_val, sizeof(export_val), "%d", index);
    write(export_file, export_val, length_export * sizeof(char));
    close(export_file);
    usleep(100000); // Sleep 100ms to make sure export is done.

    snprintf(led_path_dir,64,SYSFS_CLASS_GPIO "/gpio%d/direction",index);
    int led_dir_file = open(led_path_dir, O_RDWR);
    if (led_dir_file == -1) {
        syslog(LOG_CRIT, "ROSCUBE I: rqi_led_init unable to open GPIO direction (errno %d)", errno);
        return MRAA_ERROR_INVALID_RESOURCE;
    }
    memset(buf, 0, sizeof(buf));
    read(led_dir_file, buf, 64 * sizeof(char));
    if (strcmp(buf, "in\n") == 0) {
        // init the direction to out and set LED off
        int setup_length = snprintf(led_setup_val, sizeof(led_setup_val), "out");
        write(led_dir_file, led_setup_val, setup_length * sizeof(char));
        snprintf(led_path_val, 64, SYSFS_CLASS_GPIO "/gpio%d/value", index);
        int led_val_file = open(led_path_val, O_RDWR);
        if (led_val_file == -1) {
            syslog(LOG_CRIT, "ROSCUBE I: rqi_led_init unable to open GPIO value (errno %d)", errno);
            close(led_dir_file);
            return MRAA_ERROR_INVALID_RESOURCE;
        }
        setup_length = snprintf(led_setup_val, sizeof(led_setup_val), "1");
        write(led_val_file, led_setup_val, setup_length * sizeof(char));
        close(led_val_file);
    } 
    close(led_dir_file);
    return MRAA_SUCCESS;
}

mraa_result_t rqi_led_set_bright(int index, int val)
{
    char led_path_val[64];
    char led_setup_val[64];
    index = index_mapping(index);
    snprintf(led_path_val,64,SYSFS_CLASS_GPIO "/gpio%d/value",index);
    int led_dir_file = open(led_path_val, O_RDWR);
    if (led_dir_file == -1) {
        syslog(LOG_CRIT, "ROSCUBE I: rqi_led_set_bright unable to open GPIO value (errno %d)", errno);
        return MRAA_ERROR_INVALID_RESOURCE;
    }
    int length = snprintf(led_setup_val, sizeof(led_setup_val), "%d", (val)?0:1);
    if (write(led_dir_file, led_setup_val, length * sizeof(char)) == -1) {
        syslog(LOG_CRIT, "ROSCUBE I: rqi_led_set_bright unable to write GPIO value (errno %d)", errno);
        close(led_dir_file);
        return MRAA_ERROR_INVALID_RESOURCE;
    }
    close(led_dir_file);
    return MRAA_SUCCESS;
}

mraa_result_t rqi_led_set_close(int index)
{
    char  unexport_path[64];
    char unexport_val[64];
    index = index_mapping(index);
    snprintf(unexport_path,64,SYSFS_CLASS_GPIO "/unexport");    //int length_unexport = snprintf(unexport_val, sizeof(unexport_val), "%d", index);
    int unexport_file = open(unexport_path, O_WRONLY);
    if (unexport_file == -1) {
        syslog(LOG_CRIT, "ROSCUBE I: rqi_led_set_close unable to unexport GPIO");
        return MRAA_ERROR_INVALID_RESOURCE;
    }
    int length_unexport = snprintf(unexport_val, sizeof(unexport_val), "%d", index);
    write(unexport_file, unexport_val, length_unexport * sizeof(char));
    close(unexport_file);

    return MRAA_SUCCESS;
}

mraa_result_t rqi_led_check_bright(int index, int *val)
{
    char led_path_val[64];
    char buf[64];
    index = index_mapping(index);
    snprintf(led_path_val,64,SYSFS_CLASS_GPIO "/gpio%i/value",index);
    int led_val_file = open(led_path_val, O_RDWR);
    if (led_val_file == -1) {
        syslog(LOG_CRIT, "ROSCUBE I: rqi_led_check_bright unable to open GPIO value");
        return MRAA_ERROR_INVALID_RESOURCE;
    }
    read(led_val_file, buf, 64 * sizeof(char));
    close(led_val_file);
    *val = (atoi(buf) == 0)?1:0;
    return MRAA_SUCCESS;
}


mraa_board_t* mraa_roscube_i()
{
    int i, fd, i2c_bus_num;
    char buffer[60] = {0}, *line = NULL;
    FILE *fh;
    size_t len;

    mraa_board_t* b = (mraa_board_t*) calloc(1, sizeof (mraa_board_t));

    if (b == NULL) {
        return NULL;
    }

    b->platform_name = PLATFORM_NAME;
    b->phy_pin_count = MRAA_ROSCUBE_I_PINCOUNT;
    b->gpio_count = MRAA_ROSCUBE_GPIOCOUNT;
    b->led_dev_count = MRAA_ROSCUBE_LEDCOUNT;
    b->chardev_capable = 0;

    b->pins = (mraa_pininfo_t*) malloc(sizeof(mraa_pininfo_t) * MRAA_ROSCUBE_I_PINCOUNT);
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
    b->adv_func->led_init = rqi_led_init;
    b->adv_func->led_set_bright = rqi_led_set_bright;
    b->adv_func->led_set_close = rqi_led_set_close;
    b->adv_func->led_check_bright = rqi_led_check_bright;



    syslog(LOG_NOTICE, "ROSCubeI: base1 %d base2 %d\n", base1, base2);



    mraa_roscube_set_pininfo(b, 1,  "CN_DI0",            (mraa_pincapabilities_t){  1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 0);
    mraa_roscube_set_pininfo(b, 2,  "CN_DI1",            (mraa_pincapabilities_t){  1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 1);
    mraa_roscube_set_pininfo(b, 3,  "CN_DI2",            (mraa_pincapabilities_t){  1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 2);
    mraa_roscube_set_pininfo(b, 4,  "CN_DI3",            (mraa_pincapabilities_t){  1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 3);
    mraa_roscube_set_pininfo(b, 5,  "CN_DI4",            (mraa_pincapabilities_t){  1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 4);
    mraa_roscube_set_pininfo(b, 6,  "CN_DI5",            (mraa_pincapabilities_t){  1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 5);
    mraa_roscube_set_pininfo(b, 7,  "CN_DI6",            (mraa_pincapabilities_t){  1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 6);
    mraa_roscube_set_pininfo(b, 8,  "CN_DI7",            (mraa_pincapabilities_t){  1, 1, 0, 0, 0, 0, 0, 0 }, base1 + 7);
    mraa_roscube_set_pininfo(b, 9,  "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 10, "CON_PORT0_CAN-L",   (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 11, "CON_PORT1_CAN-L",   (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 12, "CON_I2C0_SDA",      (mraa_pincapabilities_t){  1, 0, 0, 0, 0, 1, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 13, "CON_I2C0_SCL",      (mraa_pincapabilities_t){  1, 0, 0, 0, 0, 1, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 14, "P_+5V_S_CN",        (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 15, "P_+3V3_S_CN",       (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 16, "CON_I2C1_SDA",      (mraa_pincapabilities_t){  1, 0, 0, 0, 0, 1, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 17, "CON_I2C1_SCL",      (mraa_pincapabilities_t){  1, 0, 0, 0, 0, 1, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 18, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 19, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 20, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 21, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 22, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 23, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 24, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 25, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 26, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 27, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 28, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 29, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 30, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 31, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 32, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 33, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 34, "CN_DO0",             (mraa_pincapabilities_t){  1, 1, 0, 0, 0, 0, 0, 0 }, base2 + 0);
    mraa_roscube_set_pininfo(b, 35, "CN_DO1",             (mraa_pincapabilities_t){  1, 1, 0, 0, 0, 0, 0, 0 }, base2 + 1);
    mraa_roscube_set_pininfo(b, 36, "CN_DO2",             (mraa_pincapabilities_t){  1, 1, 0, 0, 0, 0, 0, 0 }, base2 + 2);
    mraa_roscube_set_pininfo(b, 37, "CN_DO3",             (mraa_pincapabilities_t){  1, 1, 0, 0, 0, 0, 0, 0 }, base2 + 3);
    mraa_roscube_set_pininfo(b, 38, "CN_DO4",             (mraa_pincapabilities_t){  1, 1, 0, 0, 0, 0, 0, 0 }, base2 + 4);
    mraa_roscube_set_pininfo(b, 39, "CN_DO5",             (mraa_pincapabilities_t){  1, 1, 0, 0, 0, 0, 0, 0 }, base2 + 5);
    mraa_roscube_set_pininfo(b, 40, "CN_DO6",             (mraa_pincapabilities_t){  1, 1, 0, 0, 0, 0, 0, 0 }, base2 + 6);
    mraa_roscube_set_pininfo(b, 41, "CN_DO7",             (mraa_pincapabilities_t){  1, 1, 0, 0, 0, 0, 0, 0 }, base2 + 7);
    mraa_roscube_set_pininfo(b, 42, "GND",               (mraa_pincapabilities_t){ -1, 1, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 43, "CON_PORT0_CAN-H",  (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 44, "CON_PORT1_CAN-H",   (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 45, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 46, "unused",            (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 47, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 48, "unused",            (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 49, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    mraa_roscube_set_pininfo(b, 50, "GND",               (mraa_pincapabilities_t){ -1, 0, 0, 0, 0, 0, 0, 0 }, -1);
    
    
    
    b->uart_dev_count = MRAA_ROSCUBE_UARTCOUNT;
    for (int i = 0; i < MRAA_ROSCUBE_UARTCOUNT; i++)
        mraa_roscube_init_uart(b, i);
    b->def_uart_dev = 0;
    // Set number of i2c adaptors usable from userspace
    b->i2c_bus_count = 0;
    b->def_i2c_bus = 0;

    i2c_bus_num = mraa_find_i2c_bus_pci("0000:00", "0000:00:15.0","i2c_designware.0");

    if (i2c_bus_num != -1) {
        b->i2c_bus[0].bus_id = i2c_bus_num;
        mraa_roscube_get_pin_index(b, "CON_I2C0_SDA", (int*) &(b->i2c_bus[0].sda));
        mraa_roscube_get_pin_index(b, "CON_I2C0_SCL", (int*) &(b->i2c_bus[0].scl));
        b->i2c_bus_count++;
    }

    i2c_bus_num = mraa_find_i2c_bus_pci("0000:00", "0000:00:15.1","i2c_designware.1");

    if (i2c_bus_num != -1) {
        b->i2c_bus[1].bus_id = i2c_bus_num;
        mraa_roscube_get_pin_index(b, "CON_I2C1_SDA", (int*) &(b->i2c_bus[1].sda));
        mraa_roscube_get_pin_index(b, "CON_I2C1_SDA", (int*) &(b->i2c_bus[1].scl));
        b->i2c_bus_count++;
    }



    return b;

error:
    syslog(LOG_CRIT, "ROSCUBE I: Platform failed to initialise");
    free(b);
    close(_fd);
    return NULL;
}
