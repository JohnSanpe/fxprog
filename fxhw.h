/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2021-2022 John Sanpe <sanpeqf@gmail.com>
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

#define FX_RESET_REG_FX             0x7f92
#define FX_RESET_REG_FX2            0xe600

#define FX_EEPROM_MODE              0x00
#define FX_EEPROM_VENDOR            0x01
#define FX_EEPROM_PRODUCT           0x03
#define FX_EEPROM_DEVICE            0x05
#define FX_EEPROM_CONFIG            0x07
#define FX_EEPROM_HEADER            0x08
#define FX_EEPROM_FIRMWARE          0x0c

#define FX_FIRMWARE_LENH            0x00
#define FX_FIRMWARE_LENL            0x01
#define FX_FIRMWARE_ADDRH           0x02
#define FX_FIRMWARE_ADDRL           0x03
#define FX_FIRMWARE_LAST            0x80

#endif  /* _FXHW_H_ */
