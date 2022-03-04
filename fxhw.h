/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2021 Sanpe <sanpeqf@gmail.com>
 */

#ifndef _FXHW_H_
#define _FXHW_H_

#include "macro.h"

#define FX_USB_VENDOR               0x04b4
#define FX_USB_PRODUCT              0x8613
#define FX_USB_TIMEOUT              1000

#define FX_CMD_RW_INTERNAL          0xa0
#define FX_CMD_RW_EEPROM            0xa2
#define FX_CMD_RW_MEMORY            0xa3
#define FX_CMD_EEPROM_SIZE          0xa5

#define FX_ADDRESS_FX               0x7f92
#define FX_ADDRESS_FX2              0xe600

#define FX_EEPROM_MODE              0
#define FX_EEPROM_VENDOR            1
#define FX_EEPROM_PRODUCT           3
#define FX_EEPROM_DEVICE            5
#define FX_EEPROM_CONFIG            7
#define FX_EEPROM_HEADER            8
#define FX_EEPROM_FIRMWARE          12

#endif  /* _FXHW_H_ */
