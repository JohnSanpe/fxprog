/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2021-2022 John Sanpe <sanpeqf@gmail.com>
 */

#ifndef _FXPROG_H_
#define _FXPROG_H_

#include "fxhw.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

enum fxdev_type {
    DEV_TYPE_FX,
    DEV_TYPE_FX2,
    DEV_TYPE_FX2LP,
};

extern libusb_device_handle *fx_usb_device;
extern enum fxdev_type device_type;

extern int ihex_parse(const void *image, int (*fn)(uint16_t address, const void *data, size_t length, void *pdata), void *pdata);
extern int fxdev_ram_write(const void *data, size_t length, bool hex);
extern int fxdev_eeprom_info(void);
extern int fxdev_eeprom_erase(void);
extern int fxdev_eeprom_write(const void *data, size_t length, bool hex);
extern int fxdev_eeprom_mode(uint8_t mode);
extern int fxdev_eeprom_vendor(uint16_t vendor);
extern int fxdev_eeprom_product(uint16_t product);
extern int fxdev_eeprom_device(uint16_t device);
extern int fxdev_eeprom_config(uint8_t config);
extern int fxdev_eeprom_firmware(const void *data, size_t length);
extern int fxdev_reset(void);

#endif  /* _FXPROG_H_ */

