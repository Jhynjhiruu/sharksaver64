#include <libdragon.h>

#include "eeprom.h"

#include <sys/param.h>

#include "datel.h"

// EEPROM access

void wait_eeprom(void) {
    // DQ₆ and DQ₇ are both status bits, but DQ₆ is easier to deal with (and is the only option sometimes)
    // DQ₆ alternates on each read until it's finished, DQ₇ responds with the inverse of the correct bit until it's finished
    uint16_t prev = read_eeprom(0) & 0b0100000001000000;
    while (true) {
        const uint16_t next = read_eeprom(0) & 0b0100000001000000;
        if (next == prev) {
            break;
        }
        prev = next;
    }
}

void erase_sst_28lf040(void) {
    // "Chip_Erase"
    write_eeprom(0x0000, 0x3030);
    write_eeprom(0x0000, 0x3030);

    // Tsce
    wait_ms(20);

    wait_eeprom();
}

void protect_sst_28lf040(void) {
    // "Software_Data_Protect"
    read_eeprom(0x1823);
    read_eeprom(0x1820);
    read_eeprom(0x1822);
    read_eeprom(0x0418);
    read_eeprom(0x041B);
    read_eeprom(0x0419);
    read_eeprom(0x040A);
}

void unprotect_sst_28lf040(void) {
    // "Software_Data_Unprotect"
    read_eeprom(0x1823);
    read_eeprom(0x1820);
    read_eeprom(0x1822);
    read_eeprom(0x0418);
    read_eeprom(0x041B);
    read_eeprom(0x0419);
    read_eeprom(0x041A);
}

int write_sst_28lf040(void const *data, size_t len) {
    static const size_t SIZE = 0x100000;
    static const size_t PAGE_SIZE = 256;

    uint8_t const *const ptr = data;

    if (len > SIZE) {
        return FIRMWARE_TOO_LARGE;
    }

    unprotect_sst_28lf040();

    erase_sst_28lf040();

    // unlock display
    write_io(0x0600, 0x0600, 0x0600);

    uint led_status = 0;

    for (size_t written = 0; written < len; written += PAGE_SIZE * 2) {
        const size_t to_write = MIN(len - written, PAGE_SIZE * 2);

        if ((written % 0x1000) == 0) {
            // circle on the display
            for (uint i = 0; i < 8; i++) {
                const uint16_t led_data = (i == led_status) ? 0x0000 : 0x0200;
                write_io(0x0800, led_data | 0x0000, led_data | 0x0400);
            }
            led_status = (led_status + 1) % 7;
            if (led_status == 2) {
                led_status = 3;
            }
        }

        // we're writing a full page at a time, so best make sure the addresses are aligned
        // (we deal with PAGE_SIZE * 2 because there are two EEPROMs)
        assertf((written % (PAGE_SIZE * 2)) == 0, "Written bytes not aligned to page boundary\n");

        for (size_t i = 0; i < to_write; i += 2) {
            const size_t address = (written + i) / 2;
            const uint16_t data = ((uint16_t)ptr[written + i] << 8) | ((uint16_t)ptr[written + i + 1]);

            // "Byte_Program"
            write_eeprom(0x0000, 0x1010);
            write_eeprom(address, data);

            // do we need some delay in here??? i hope not!

            wait_eeprom();
        }
    }

    for (uint i = 0; i < 8; i++) {
        write_io(0x0800, 0x0200, 0x0600);
    }

    protect_sst_28lf040();

    return 0;
}

void erase_sst_29le010(void) {
    // "Software Chip Erase"
    write_eeprom(0x5555, 0xAAAA);
    write_eeprom(0x2AAA, 0x5555);
    write_eeprom(0x5555, 0x8080);
    write_eeprom(0x5555, 0xAAAA);
    write_eeprom(0x2AAA, 0x5555);
    write_eeprom(0x5555, 0x1010);

    // Tsce
    wait_ms(20);

    wait_eeprom();
}

void protect_sst_29le010(void) {
    // "Software Data Protect Enable & Page Write"
    write_eeprom(0x5555, 0xAAAA);
    write_eeprom(0x2AAA, 0x5555);
    write_eeprom(0x5555, 0xA0A0);

    // important: you must now WAIT 200μs (and ideally call wait_eeprom)
    // before doing anything else except programming data!
}

void unprotect_sst_29le010(void) {
    // "Software Data Protect Disable"
    write_eeprom(0x5555, 0xAAAA);
    write_eeprom(0x2AAA, 0x5555);
    write_eeprom(0x5555, 0x8080);
    write_eeprom(0x5555, 0xAAAA);
    write_eeprom(0x2AAA, 0x5555);
    write_eeprom(0x5555, 0x2020);

    // Tblco
    wait_ms(1);

    wait_eeprom();
}

int write_sst_29le010(void const *data, size_t len) {
    static const size_t SIZE = 0x40000;
    static const size_t PAGE_SIZE = 128;

    uint8_t const *const ptr = data;

    if (len > SIZE) {
        return FIRMWARE_TOO_LARGE;
    }

    unprotect_sst_29le010();

    erase_sst_29le010();

    // unlock display
    write_io(0x0600, 0x0600, 0x0600);

    uint led_status = 0;

    for (size_t written = 0; written < len; written += PAGE_SIZE * 2) {
        const size_t to_write = MIN(len - written, PAGE_SIZE * 2);

        if ((written % 0x1000) == 0) {
            // circle on the display
            for (uint i = 0; i < 8; i++) {
                const uint16_t led_data = (i == led_status) ? 0x0000 : 0x0200;
                write_io(0x0800, led_data | 0x0000, led_data | 0x0400);
            }
            led_status = (led_status + 1) % 7;
            if (led_status == 2) {
                led_status = 3;
            }
        }

        // we're writing a full page at a time, so best make sure the addresses are aligned
        // (we deal with PAGE_SIZE * 2 because there are two EEPROMs)
        assertf((written % (PAGE_SIZE * 2)) == 0, "Written bytes not aligned to page boundary\n");

        for (size_t i = 0; i < to_write; i += 2) {
            const size_t address = (written + i) / 2;
            const uint16_t data = ((uint16_t)ptr[written + i] << 8) | ((uint16_t)ptr[written + i + 1]);
            write_eeprom(address, data);
        }

        // Twc
        wait_ms(1);

        wait_eeprom();

        // check DQ₇ to be sure we're definitely finished - the datasheet says the timing can be funky sometimes
        const uint16_t expected = (((uint16_t)ptr[written + to_write - 2] << 8) | ((uint16_t)ptr[written + to_write - 1])) & 0b1000000010000000;
        while ((read_eeprom((written + to_write - 2) / 2) & 0b1000000010000000) != expected) {
            continue;
        }
    }

    for (uint i = 0; i < 8; i++) {
        write_io(0x0800, 0x0200, 0x0600);
    }

    protect_sst_29le010();

    // Twc
    wait_ms(1);

    wait_eeprom();

    return 0;
}

uint32_t read_ids_alt(void) {
    // "Read_ID"
    write_eeprom(0x0000, 0x9090);

    const uint16_t ids_lo = read_eeprom(0x0000);
    const uint16_t ids_hi = read_eeprom(0x0001);

    // "Reset"
    write_eeprom(0x0000, 0xFFFF);

    return ((uint32_t)ids_lo << 16) | ((uint32_t)ids_hi);
}

uint32_t read_ids(void) {
    // "Software ID Entry"
    write_eeprom(0x5555, 0xAAAA);
    write_eeprom(0x2AAA, 0x5555);
    write_eeprom(0x5555, 0x8080);
    write_eeprom(0x5555, 0xAAAA);
    write_eeprom(0x2AAA, 0x5555);
    write_eeprom(0x5555, 0x6060);

    const uint16_t ids_lo = read_eeprom(0x0000);
    const uint16_t ids_hi = read_eeprom(0x0001);

    // "Software ID Exit"
    write_eeprom(0x5555, 0xAAAA);
    write_eeprom(0x2AAA, 0x5555);
    write_eeprom(0x5555, 0xF0F0);

    if (ids_lo != 0xBFBF) {
        return read_ids_alt();
    } else {
        return ((uint32_t)ids_lo << 16) | ((uint32_t)ids_hi);
    }
}
