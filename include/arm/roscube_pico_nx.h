/*
 * Author: Author: Gowtham R <gowtham.r@adlinktech.com>
 * Based on work from: Dan O'Donovan <dan@emutex.com>
 *                     Nicola Lunghi <nicola.lunghi@emutex.com>
 * Copyright (c) 2017 Emutex Ltd.
 * Copyright (c) 2014 Intel Corporation.
 * Copyright (c) 2019 ADLINK Technology Inc.
 * SPDX-License-Identifier: MIT   
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "mraa_internal.h"

// +1 as pins are "1 indexed"
#define MRAA_ROSCUBE_PICO_NX_PINCOUNT    (37 + 1)

mraa_board_t*
mraa_roscube_pico_nx();

#ifdef __cplusplus
}
#endif
