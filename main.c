/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2021 Sanpe <sanpeqf@gmail.com>
 */

#include "fxprog.h"
#include <fcntl.h>
#include <err.h>
#include <getopt.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

static int firmware_fd;
static void *firmware_data;
static struct stat firmware_stat;

enum flags_bit {
    __FLAG_INFO,
    __FLAG_ERASE,
    __FLAG_FLASH,
    __FLAG_MODE,
    __FLAG_VENDOR,
    __FLAG_PRODUCT,
    __FLAG_DEVICE,
    __FLAG_CONFIG,
    __FLAG_FIRMWARE,
    __FLAG_MEMORY,
    __FLAG_RESET,
};

#define FLAG_INFO       (1LU << __FLAG_INFO)
#define FLAG_ERASE      (1LU << __FLAG_ERASE)
#define FLAG_FLASH      (1LU << __FLAG_FLASH)
#define FLAG_MODE       (1LU << __FLAG_MODE)
#define FLAG_VENDOR     (1LU << __FLAG_VENDOR)
#define FLAG_PRODUCT    (1LU << __FLAG_PRODUCT)
#define FLAG_DEVICE     (1LU << __FLAG_DEVICE)
#define FLAG_CONFIG     (1LU << __FLAG_CONFIG)
#define FLAG_FIRMWARE   (1LU << __FLAG_FIRMWARE)
#define FLAG_MEMORY     (1LU << __FLAG_MEMORY)
#define FLAG_RESET      (1LU << __FLAG_RESET)

static const struct option options[] = {
    {"help",        no_argument,        0,  'h'},
    {"device",      required_argument,  0,  'd'},
    {"port",        required_argument,  0,  'p'},
    {"preload",     required_argument,  0,  'l'},
    {"info",        no_argument,        0,  'i'},
    {"erase",       no_argument,        0,  'e'},
    {"flash",       required_argument,  0,  'w'},
    {"bootmode",    required_argument,  0,  'B'},
    {"vendor",      required_argument,  0,  'V'},
    {"product",     required_argument,  0,  'P'},
    {"device",      required_argument,  0,  'D'},
    {"firmware",    required_argument,  0,  'F'},
    {"memory",      required_argument,  0,  'm'},
    {"reset",       no_argument,        0,  'r'},
    {"version",     no_argument,        0,  'v'},
    { }, /* NULL */
};

static __noreturn void usage(void)
{
    printf("Usage: fxprog [options]...\n");
    printf("  -h, --help                    display this message                    \n");
    printf("  -d, --device      <type>      device type: fx fx2 fx2lp               \n");
    printf("  -p, --port        <vid:pid>   set device vendor and product           \n");
    printf("  -l, --preload     <file>      load preload to memory                  \n");
    printf("  -i, --info                    read the eeprom info                    \n");
    printf("  -e, --erase                   erase the entire eeprom                 \n");
    printf("  -w, --flash       <file>      write eeprom with data from filename    \n");
    printf("  -B, --bootmode    <mode>      write bootmode to eeprom                \n");
    printf("  -V, --vendor      <vid>       write vendor id to eeprom               \n");
    printf("  -P, --product     <pid>       write product id to eeprom              \n");
    printf("  -D, --device      <did>       write device id to eeprom               \n");
    printf("  -C, --config      <conf>      write config to eeprom                  \n");
    printf("  -F, --firmware    <file>      write firmware to eeprom                \n");
    printf("  -m, --memory      <file>      load memory with data from filename     \n");
    printf("  -r, --reset                   reset chip after operate                \n");
    printf("  -v, --version                 display version information             \n");
    exit(1);
}

static __noreturn void version(void)
{
    printf("Fxprog v1.0\n");
    printf("Copyright(c) 2021 Sanpe <sanpeqf@gmail.com>\n");
    printf("License GPLv2+: GNU GPL version 2 or later.\n");
    exit(1);
}

static int fx_usb_init(uint16_t usb_vendor, uint16_t usb_product)
{
    int retval;

    if ((retval = libusb_init(NULL))) {
        fprintf(stderr, "Cannot initialize libusb: %s\n", libusb_error_name(retval));
        return retval;
    };

    fx_usb_device = libusb_open_device_with_vid_pid(NULL, usb_vendor, usb_product);
    if (!fx_usb_device) {
        fprintf(stderr, "Cannot found bootloader mode chip\n");
        return -ENODEV;
    }

    if (libusb_kernel_driver_active(fx_usb_device, 0)) {
        if ((retval = libusb_detach_kernel_driver(fx_usb_device, 0))) {
            fprintf(stderr, "Cannot to detach kernel driver: %s\n", libusb_error_name(retval));
            return retval;
        }
    }

    if ((retval = libusb_claim_interface(fx_usb_device, 0))) {
        fprintf(stderr, "Cannot claim interface: %s\n", libusb_error_name(retval));
        return retval;
    };

    return 0;
}

static inline bool file_is_hex(const char *file)
{
    return strstr(file, ".hex") || strstr(file, ".ihx");
}

static bool mmap_firmware(const char *file)
{
    int retval;
    bool hex;

    if ((firmware_fd = open(file, O_RDONLY)) < 0)
        err(-1, "Cannot open file: %s", file);

    if ((retval = fstat(firmware_fd, &firmware_stat)) < 0)
        err(retval, "file fstat err");

    firmware_data = mmap(NULL, firmware_stat.st_size, PROT_READ, MAP_SHARED, firmware_fd, 0);
    if (firmware_data == MAP_FAILED)
        err(-1, "file mmap err");

    if (file_is_hex(file))
        hex = true;
    else
        hex = false;

    return hex;
}

int main(int argc, char *const argv[])
{
    uint16_t usb_vendor = FX_USB_VENDOR;
    uint16_t usb_product = FX_USB_PRODUCT;
    const char *preload = "preload.hex";
    const char *flash, *memory, *firmware;
    unsigned long flags = 0;
    uint16_t vendor, product, device;
    uint8_t mode, config;
    int optidx, retval;
    char arg, *tmp;
    bool hex;

    while ((arg = getopt_long(argc, argv, "hd:p:l:iew:B:V:P:D:C:F:m:rv", options, &optidx)) != -1) {
        switch (arg) {
            case 'd':
                if (!strcmp(optarg, "fx"))
                    device_type = DEV_TYPE_FX;
                else if (!strcmp(optarg, "fx2"))
                    device_type = DEV_TYPE_FX2;
                else if (!strcmp(optarg, "fx2lp"))
                    device_type = DEV_TYPE_FX2LP;
                else
                    usage();
                break;

            case 'p':
                tmp = strchr(optarg, ':');
                *tmp++ = '\0';
                usb_vendor = strtoul(optarg, NULL, 0);
                usb_product = strtoul(tmp, NULL, 0);
                break;

            case 'l':
                preload = optarg;
                break;

            case 'i':
                flags |= FLAG_INFO;
                break;

            case 'e':
                flags |= FLAG_ERASE;
                break;

            case 'w':
                flags |= FLAG_FLASH;
                flash = optarg;
                break;

            case 'B':
                flags |= FLAG_MODE;
                mode = strtoul(optarg, NULL, 0);
                break;

            case 'V':
                flags |= FLAG_VENDOR;
                vendor = strtoul(optarg, NULL, 0);
                break;

            case 'P':
                flags |= FLAG_PRODUCT;
                product = strtoul(optarg, NULL, 0);
                break;

            case 'D':
                flags |= FLAG_DEVICE;
                device = strtoul(optarg, NULL, 0);
                break;

            case 'C':
                flags |= FLAG_CONFIG;
                config = strtoul(optarg, NULL, 0);
                break;

            case 'F':
                if (file_is_hex(optarg))
                    usage();
                flags |= FLAG_FIRMWARE;
                firmware = optarg;
                break;

            case 'm':
                flags |= FLAG_MEMORY;
                memory = optarg;
                break;

            case 'r':
                flags |= FLAG_RESET;
                break;

            case 'v':
                version();

            case 'h': default:
                usage();
        }
    }

    if (argc < 2)
        usage();

    printf("Fxprog v1.0\n");

    retval = fx_usb_init(usb_vendor, usb_product);
    if (retval)
        return retval;

    hex = mmap_firmware(preload);
    fxdev_ram_write(firmware_data, firmware_stat.st_size, hex);

    if ((flags & FLAG_INFO) && (retval = fxdev_eeprom_info()))
        err(retval, "Failed to read the eeprom info");

    if ((flags & FLAG_ERASE) && (retval = fxdev_eeprom_erase()))
        err(retval, "Failed to erase the entire eeprom");

    if (flags & FLAG_FLASH) {
        hex = mmap_firmware(flash);
        retval = fxdev_eeprom_write(firmware_data, firmware_stat.st_size, hex);
        if (retval)
            err(retval, "Failed to write eeprom with data");
    }

    if ((flags & FLAG_MODE) && (retval = fxdev_eeprom_mode(mode)))
        err(retval, "Failed to write bootmode");

    if ((flags & FLAG_VENDOR) && (retval = fxdev_eeprom_vendor(vendor)))
        err(retval, "Failed to get write vendor");

    if ((flags & FLAG_PRODUCT) && (retval = fxdev_eeprom_product(product)))
        err(retval, "Failed to get write product");

    if ((flags & FLAG_DEVICE) && (retval = fxdev_eeprom_device(device)))
        err(retval, "Failed to get write device");

    if ((flags & FLAG_CONFIG) && (retval = fxdev_eeprom_config(config)))
        err(retval, "Failed to get write config");

    if (flags & FLAG_FIRMWARE) {
        hex = mmap_firmware(firmware);
        retval = fxdev_eeprom_firmware(firmware_data, firmware_stat.st_size);
        if (retval)
            err(retval, "Failed to write firmware");
    }

    if (flags & FLAG_MEMORY) {
        hex = mmap_firmware(memory);
        retval = fxdev_ram_write(firmware_data, firmware_stat.st_size, hex);
        if (retval)
            err(retval, "Failed to load memory with data");
    }

    if ((flags & FLAG_RESET) && (retval = fxdev_reset()))
        err(retval, "Failed to reset chip");

    return 0;
}