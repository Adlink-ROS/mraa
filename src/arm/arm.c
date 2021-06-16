/*
 * Author: Thomas Ingleby <thomas.c.ingleby@intel.com>
 * Author: Michael Ring <mail@michael-ring.org>
 * Copyright (c) 2014 Intel Corporation.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <string.h>

#include "arm/96boards.h"
#include "arm/rockpi4.h"
#include "arm/de_nano_soc.h"
#include "arm/banana.h"
#include "arm/beaglebone.h"
#include "arm/phyboard.h"
#include "arm/raspberry_pi.h"
#include "arm/adlink_ipi.h"
#include "mraa_internal.h"
#include "arm/roscube_x_580.h"
#include "arm/roscube_x_58g.h"
#include "arm/roscube_pico_npn1.h"
#include "arm/roscube_pico_npn2.h"
#include "arm/roscube_pico_npn3.h"


mraa_platform_t
mraa_arm_platform()
{
    mraa_platform_t platform_type = MRAA_UNKNOWN_PLATFORM;
    size_t len = 100;
    FILE *fh = fopen("/proc/device-tree/model", "r");;
    char model_name[200];
    fgets(model_name, 200, fh); 
    fclose(fh);
    if (strcmp(model_name , "ADLINK ROScube-Pico Nano Development Kit") == 0){
        platform_type = MRAA_ADLINK_ROSCUBE_PICO_NPN1;
    } else if (strcmp(model_name, "ADLINK ROScube-Pico NX Development Kit") == 0) {
        platform_type = MRAA_ADLINK_ROSCUBE_PICO_NPN2;
    } else if (strcmp(model_name, "ADLINK ROScube-Pico TX2NX Development Kit") == 0) {
        platform_type = MRAA_ADLINK_ROSCUBE_PICO_NPN3;
    } else if (strcmp(model_name, "ADLINK ROScube-X 580 Robotic Controller") == 0) {
        platform_type = MRAA_ADLINK_ROSCUBE_X_580;
    } else if (strcmp(model_name, "ADLINK ROScube-X 58G Robotic Controller") == 0) {
        platform_type = MRAA_ADLINK_ROSCUBE_X_58G;
    }
    else{
        platform_type = MRAA_UNKNOWN_PLATFORM;
    }
        


    /* Get compatible string from Device tree for boards that dont have enough info in /proc/cpuinfo
     */
    if (platform_type == MRAA_UNKNOWN_PLATFORM) {
        if (mraa_file_contains("/proc/device-tree/model", "s900"))
            platform_type = MRAA_96BOARDS;
        else if (mraa_file_contains("/proc/device-tree/compatible", "qcom,apq8016-sbc"))
            platform_type = MRAA_96BOARDS;
        else if (mraa_file_contains("/proc/device-tree/compatible", "arrow,apq8096-db820c"))
            platform_type = MRAA_96BOARDS;
        else if (mraa_file_contains("/proc/device-tree/model",
                                    "HiKey Development Board"))
            platform_type = MRAA_96BOARDS;
        else if (mraa_file_contains("/proc/device-tree/model", "HiKey960"))
            platform_type = MRAA_96BOARDS;
        else if (mraa_file_contains("/proc/device-tree/model", "ROCK960"))
            platform_type = MRAA_96BOARDS;
        else if (mraa_file_contains("/proc/device-tree/model", "ZynqMP ZCU100 RevC"))
            platform_type = MRAA_96BOARDS;
        else if (mraa_file_contains("/proc/device-tree/model", "Avnet Ultra96 Rev1"))
            platform_type = MRAA_96BOARDS;
        else if (mraa_file_contains("/proc/device-tree/model", "ROCK Pi 4")  ||
                 mraa_file_contains("/proc/device-tree/model", "ROCK PI 4A") ||
                 mraa_file_contains("/proc/device-tree/model", "ROCK PI 4B")
                 )
            platform_type = MRAA_ROCKPI4;
        else if (mraa_file_contains("/proc/device-tree/compatible", "raspberrypi,"))
            platform_type = MRAA_RASPBERRY_PI;
        else if (mraa_file_contains("/proc/device-tree/model", "ADLINK ARM, LEC-PX30"))
            platform_type = MRAA_ADLINK_IPI;
    }

    switch (platform_type) {
        case MRAA_RASPBERRY_PI:
            plat = mraa_raspberry_pi();
            break;
        case MRAA_BEAGLEBONE:
            plat = mraa_beaglebone();
            break;
        case MRAA_PHYBOARD_WEGA:
            plat = mraa_phyboard();
            break;
        case MRAA_BANANA:
            plat = mraa_banana();
            break;
        case MRAA_96BOARDS:
            plat = mraa_96boards();
	    break;
        case MRAA_ROCKPI4:
	    plat = mraa_rockpi4();
            break;
        case MRAA_DE_NANO_SOC:
            plat = mraa_de_nano_soc();
            break;
	    case MRAA_ADLINK_IPI:
	        plat = mraa_adlink_ipi();
	        break;
	    case MRAA_ADLINK_ROSCUBE_X_580:
	        plat = mraa_roscube_x_580();
	        break;
        case MRAA_ADLINK_ROSCUBE_X_58G:
            plat = mraa_roscube_x_58g();
            break;
        case MRAA_ADLINK_ROSCUBE_PICO_NPN1:
            plat = mraa_roscube_pico_npn1();
            break;
        case MRAA_ADLINK_ROSCUBE_PICO_NPN2:
            plat = mraa_roscube_pico_npn2();
            break;
        case MRAA_ADLINK_ROSCUBE_PICO_NPN3:
            plat = mraa_roscube_pico_npn3();
            break;
        default:
            plat = NULL;
            syslog(LOG_ERR, "Unknown Platform, currently not supported by MRAA");
    }
    return platform_type;
}
