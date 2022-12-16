/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2021-2022 John Sanpe <sanpeqf@gmail.com>
 */

#include "fxprog.h"
#include <unistd.h>

libusb_device_handle *fx_usb_device;
enum fxdev_type device_type;
typedef bool (*is_external_t)(uint16_t addr, size_t length);

static int ezusb_read(const char *label, uint8_t opcode,
                      uint16_t addr, uint8_t *data, size_t len)
{
    int retlen;

    retlen = libusb_control_transfer(
        fx_usb_device,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
        LIBUSB_RECIPIENT_DEVICE,
        opcode, addr, 0, data, len, FX_USB_TIMEOUT
    );

    if (retlen < 0) {
        fprintf(
            stderr, "ezusb_read '%s' failed: %s\n",
            label, libusb_error_name(retlen)
        );
        return retlen;
    }

    return 0;
}

static int ezusb_write(const char *label, uint8_t opcode,
                       uint16_t addr, const void *data, size_t len)
{
    int retlen;

    retlen = libusb_control_transfer(
        fx_usb_device,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
        LIBUSB_RECIPIENT_DEVICE,
        opcode, addr, 0, (void *)data, len, FX_USB_TIMEOUT
    );

    if (retlen < 0) {
        fprintf(
            stderr, "ezusb_write '%s' failed: %s\n",
            label, libusb_error_name(retlen)
        );
        return retlen;
    }

    return 0;
}

static uint16_t ezusb_reset_reg(void)
{
    uint16_t address;

    switch (device_type) {
        case DEV_TYPE_FX:
            address = FX_RESET_REG_FX;
            break;

        case DEV_TYPE_FX2 ... DEV_TYPE_FX2LP:
            address = FX_RESET_REG_FX2;
            break;
    }

    return address;
}

static int ezusb_reset(bool enable)
{
    uint16_t address;
    int retval;

    address = ezusb_reset_reg();

    retval = ezusb_write(
        "ezusb_reset", FX_CMD_RW_INTERNAL,
        address, (void *)&enable, 1
    );

    return retval;
}

static int ezusb_ram_write(uint16_t address, const void *data, size_t length, void *pdata)
{
    is_external_t is_external = pdata;
    unsigned int retry = 6;
    bool external;
    int retval;

    external = is_external(address, length);

    while (--retry) {
        if (!(retval = ezusb_write(
            "ezusb_ram_write",
            external ? FX_CMD_RW_MEMORY : FX_CMD_RW_INTERNAL,
            address, data, length
        )))
            break;
    }

    return retval;
}

static int ezusb_eeprom_write(uint16_t address, const void *data, size_t length, void *pdata)
{
    unsigned int retry = 6;
    int retval;

    while (--retry) {
        if (!(retval = ezusb_write(
            "ezusb_eeprom_write",
            FX_CMD_RW_EEPROM,
            address, data, length
        )))
            break;
    }

    return retval;
}

static bool fx_is_external(uint16_t addr, size_t length)
{
    uint16_t end = addr + length;

    /* check download address */
    if (end < addr) {
        fprintf(stderr, "Download address surround\n");
        exit(-EFAULT);
    }

    /* 1st 16KB for data/code, 0x0000-0x1b3f */
    if (addr < 0x1b40)
        return end > 0x1b40;

    /* otherwise, it's certainly external */
    else
        return true;
}

static bool fx2_is_external(uint16_t addr, size_t length)
{
    uint16_t end = addr + length;

    /* check download address */
    if (end < addr) {
        fprintf(stderr, "Download address surround\n");
        exit(-EFAULT);
    }

    /* 1st 8KB for data/code, 0x0000-0x1fff */
    if (addr < 0x2000)
        return end > 0x2000;

    /* and 512 for data, 0xe000-0xe1ff */
    else if (addr >= 0xe000 && addr < 0xe200)
        return end > 0xe200;

    /* otherwise, it's certainly external */
    else
        return true;
}

static bool fx2lp_is_external(uint16_t addr, size_t length)
{
    uint16_t end = addr + length;

    if (end < addr) {
        fprintf(stderr, "Download address surround\n");
        exit(-EFAULT);
    }

    /* 1st 16KB for data/code, 0x0000-0x3fff */
    if (addr < 0x4000)
        return end > 0x4000;

    /* and 512 for data, 0xe000-0xe1ff */
    else if (addr >= 0xe000 && addr < 0xe200)
        return end > 0xe200;

    /* otherwise, it's certainly external */
    else
        return true;
}

int fxdev_ram_write(const void *data, size_t length, bool hex)
{
    is_external_t is_external;
    int retval;

    switch (device_type) {
        case DEV_TYPE_FX:
            is_external = fx_is_external;
            break;

        case DEV_TYPE_FX2:
            is_external = fx2_is_external;
            break;

        case DEV_TYPE_FX2LP:
            is_external = fx2lp_is_external;
            break;
    }

    /* don't let CPU run while we overwrite its code/data */
    if ((retval = ezusb_reset(true)))
        return retval;

    if (hex)
        retval = ihex_parse(data, ezusb_ram_write, is_external);
    else
        retval = ezusb_ram_write(0, data, length, is_external);
    if (retval)
        return retval;

    if ((retval = ezusb_reset(false)))
        return retval;

    return 0;
}

int fxdev_eeprom_info(void)
{
    uint8_t info;
    int retval;

    printf("Chip ID:\n");

    retval = ezusb_read(
        "fxdev_eeprom_info",
        FX_CMD_EEPROM_SIZE,
        0, &info, 1
    );

    if (retval)
        return retval;

    printf("  eeprom info: 0x%02x\n", info);
    printf("  Done!\n");
    return 0;
}

int fxdev_eeprom_erase(void)
{
    uint8_t data[16];
    int retval;


    printf("Chip erase eeprom...\n");

    memset(data, 0xff, sizeof(data));
    retval = ezusb_eeprom_write(FX_EEPROM_MODE, &data, 16, NULL);
    if (retval)
        return retval;

    printf("  Done!\n");
    return 0;
}

int fxdev_eeprom_write(const void *data, size_t length, bool hex)
{
    int retval;

    printf("Chip write eeprom...\n");
    printf("  Length: 0x%04lx\n", length);

    if (hex)
        retval = ihex_parse(data, ezusb_eeprom_write, NULL);
    else
        retval = ezusb_eeprom_write(0, data, length, NULL);
    if (retval)
        return retval;

    printf("  Done!\n");
    return 0;
}

int fxdev_eeprom_mode(uint8_t mode)
{
    int retval;

    printf("Chip write bootmode...\n");
    printf("  Boot mode: 0x%02x\n", mode);

    retval = ezusb_eeprom_write(FX_EEPROM_MODE, &mode, 1, NULL);
    if (retval)
        return retval;

    printf("  Done!\n");
    return 0;
}

int fxdev_eeprom_vendor(uint16_t vendor)
{
    int retval;

    printf("Chip write vendor...\n");
    printf("  Vendor ID: 0x%04x\n", vendor);

    retval = ezusb_eeprom_write(FX_EEPROM_VENDOR, &vendor, 2, NULL);
    if (retval)
        return retval;

    printf("  Done!\n");
    return 0;
}

int fxdev_eeprom_product(uint16_t product)
{
    int retval;

    printf("Chip write product...\n");
    printf("  Product ID: 0x%04x\n", product);

    retval = ezusb_eeprom_write(FX_EEPROM_PRODUCT, &product, 2, NULL);
    if (retval)
        return retval;

    printf("  Done!\n");
    return 0;
}

int fxdev_eeprom_device(uint16_t device)
{
    int retval;

    printf("Chip write device...\n");
    printf("  Device ID: 0x%04x\n", device);

    retval = ezusb_eeprom_write(FX_EEPROM_DEVICE, &device, 2, NULL);
    if (retval)
        return retval;

    printf("  Done!\n");
    return 0;
}

int fxdev_eeprom_config(uint8_t config)
{
    int retval;

    printf("Chip write config...\n");
    printf("  Config: 0x%02x\n", config);

    retval = ezusb_eeprom_write(FX_EEPROM_DEVICE, &config, 1, NULL);
    if (retval)
        return retval;

    printf("  Done!\n");
    return 0;
}

int fxdev_eeprom_firmware(const void *data, size_t length)
{
    uint16_t address;
    uint8_t transfer[8] = {};
    int retval;

    printf("Chip write firmware...\n");
    printf("  Length: 0x%04lx\n", length);

    transfer[FX_FIRMWARE_LENH] = length >> 8;
    transfer[FX_FIRMWARE_LENL] = length;

    retval = ezusb_eeprom_write(FX_EEPROM_HEADER, transfer, 4, NULL);
    if (retval)
        return retval;

    retval = ezusb_eeprom_write(FX_EEPROM_FIRMWARE, data, length, NULL);
    if (retval)
        return retval;

    address = ezusb_reset_reg();

    transfer[FX_FIRMWARE_LENH] = FX_FIRMWARE_LAST;
    transfer[FX_FIRMWARE_LENL] = 0x01;
    transfer[FX_FIRMWARE_ADDRH] = address >> 8;
    transfer[FX_FIRMWARE_ADDRL] = address;

    retval = ezusb_eeprom_write(FX_EEPROM_FIRMWARE + length, transfer, 8, NULL);
    if (retval)
        return retval;

    printf("  Done!\n");
    return 0;
}

int fxdev_reset(void)
{
    int retval;

    printf("Chip reset...\n");

    if ((retval = ezusb_reset(true)))
        return retval;

    usleep(100);

    if ((retval = ezusb_reset(false)))
        return retval;

    printf("  Done!\n");
    return 0;
}
