/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright(c) 2021 Sanpe <sanpeqf@gmail.com>
 */

#include "fxprog.h"
#include <ctype.h>

struct ihex_head{
    char code;
    char length[2];
    char offset[4];
    char type[2];
    char data[0];
} __packed;

enum ihex_type {
    IHEX_TYPE_DATA      = 0,
    IHEX_TYPE_EOF       = 1,
    IHEX_TYPE_ESEG      = 2,
    IHEX_TYPE_SSEG      = 3,
    IHEX_TYPE_EADDR     = 4,
    IHEX_TYPE_SADDR     = 5,
};

static unsigned int strtohex(const char *str, unsigned int length)
{
    unsigned int value;

    for (value = 0; length; --length) {
        if (*str >= '0' && *str <= '9')
            value = (value * 16) + (*str++ - '0');
        else if (tolower(*str) >= 'a' && tolower(*str) <= 'f')
            value = (value * 16) + (tolower(*str++) - 'a' + 10);
        else
            return 0;
    }

    return value;
}

bool ihex_checksum(const char *start, const char *end)
{
    uint8_t cumul;

    for (cumul = 0; start < end; start += 2)
        cumul += strtohex(start, 2);

    cumul = 0x100 - cumul;
    return strtohex(end, 2) == cumul;
}

int ihex_parse(const void *image, int (*fn)(uint16_t address, const void *data, size_t length, void *pdata), void *pdata)
{
    const struct ihex_head *line, *next;
    unsigned int base;
    char buff[255];
    int retval;

    for (line = image; line; line = next) {
        unsigned int length, offset, type;
        unsigned int count;
        const char *end;
        uint32_t addr;

        if (line->code == '#')
            continue;

        if (line->code != ':') {
            fprintf(stderr, "Error IHEX format\n");
            return -EINVAL;
        }

        end = strchr(line->data, '\n');
        if (!end) {
            fprintf(stderr, "EOF without EOF record\n");
            return -ENFILE;
        }

        next = (void *)end + 1;

        /* Get ihex line information */
        length = strtohex(&line->length[0], 2);
        offset = strtohex(&line->offset[0], 4);
        type = strtohex(&line->type[0], 2);
        addr = ((base & 0xffff) << 16) | (offset & 0xffff);

        /* Check the actual length of the data */
        if (((end - line->data - 2) / 2) != length) {
            fprintf(stderr, "Error IHEX length\n");
            return -EFAULT;
        }

        if (!ihex_checksum((void *)line + 1, (void *)end - 2)) {
            fprintf(stderr, "Error IHEX checksum\n");
            return -EFAULT;
        }

        switch (type) {
            case IHEX_TYPE_DATA:
                for (count = 0; count < length; ++count)
                    buff[count] = strtohex(&line->data[count * 2], 2);
                retval = fn(addr, buff, length, pdata);
                if (retval)
                    return retval;
                break;

            case IHEX_TYPE_EADDR:
                if (length != 4) {
                    fprintf(stderr, "Error IHEX addr format\n");
                    return -ENODATA;
                }
                base = strtohex(&line->data[0], 4);
                break;

            case IHEX_TYPE_EOF:
                return 0;

            default:
                continue;
        }
    }

    return -ENFILE;
}
