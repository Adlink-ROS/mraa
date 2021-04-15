/*
 * Author: Manivannan Sadhasivam <manivannan.sadhasivam@linaro.org>
 * Copyright (c) 2018, Linaro Ltd.
 *
 * SPDX-License-Identifier: MIT
 *
 * Example usage: Reads maximum brightness value for user led and turns it
 *                on/off depending on current state. Then sets led trigger
 *                to heartbeat.
 *
 */

/* standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* mraa header */
#include "mraa/led.h"

/* LED default value */
#define LED_NUM    0
#define LED_ON_OFF 0

/* trigger type */
#define LED_TRIGGER "heartbeat"

int
main(int argc, char *argv[])
{
    mraa_result_t status = MRAA_SUCCESS;
    mraa_led_context led;
    int led_num, led_on_off;
    led_num = (argc >= 2)?atoi(argv[1]):LED_NUM;
    led_on_off = (argc >= 3)?atoi(argv[2]):LED_ON_OFF;
    printf("Will set LED %d to %d\n", led_num, led_on_off);
    
    /* initialize mraa for the platform (not needed most of the time) */
    mraa_init();
 
    //! [Interesting]
    /* initialize LED */
    led = mraa_led_init(led_num);
    if (led == NULL) {
        fprintf(stderr, "Failed to initialize LED\n");
        mraa_deinit();
        return EXIT_FAILURE;
    }

    /* read maximum brightness */
    int val = mraa_led_read_max_brightness(led);
    if (val >= 0) {
        fprintf(stdout, "maximum brightness value for LED is: %d\n", val);
    } else {
        fprintf(stdout, "maximum brightness is not supported.\n");
    }

    /* turn LED on/off depending on max_brightness value */
    status = mraa_led_set_brightness(led, led_on_off);
    if (status != MRAA_SUCCESS) {
        fprintf(stderr, "unable to set LED brightness\n");
        goto err_exit;
    }

    /* sleep for 1 seconds */
    sleep(1);

    /* set LED trigger to heartbeat */
    status = mraa_led_set_trigger(led, LED_TRIGGER);
    if (status == MRAA_SUCCESS) {
        fprintf(stdout, "LED trigger set to: heartbeat\n");
    } else {
        fprintf(stderr, "unable to set LED trigger to: heartbeat\n");
    }

    /* close LED */
    mraa_led_close(led);

    //! [Interesting]
    /* deinitialize mraa for the platform (not needed most of the times) */
    mraa_deinit();

    return EXIT_SUCCESS;

err_exit:
    mraa_result_print(status);

    /* deinitialize mraa for the platform (not needed most of the times) */
    mraa_deinit();

    return EXIT_FAILURE;
}
