#include <libdragon.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <malloc.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

static const char PATH[] = "sd:/fw.bin";
static const char ROM_PATH[] = "rom:/fw.bin";

static void reset_handler_callback(void) {
    printf("Reset button pressed, waiting for exception...\n");
    console_render();
}

// GameShark interface code

static uint8_t GS_BASE = 0x10;

static const uint8_t IO_BASE = 0x40;

static uint32_t read_gs(uint32_t addr) {
    const uint32_t masked = addr & 0x00FFFFFF;

    const uint32_t pi_addr = ((uint32_t)GS_BASE << 24) | (masked);

    assertf(io_accessible(pi_addr), "PI address out of range\n");

    return io_read(pi_addr);
}

static void write_gs(uint32_t addr, uint32_t data) {
    const uint32_t masked = addr & 0x00FFFFFF;

    const uint32_t pi_addr = ((uint32_t)GS_BASE << 24) | (masked);

    assertf(io_accessible(pi_addr), "PI address out of range\n");

    return io_write(pi_addr, data);
}

static uint32_t read_io(uint16_t addr) {
    const uint32_t gs_addr = ((uint32_t)IO_BASE << 16) | ((uint32_t)addr);

    return read_gs(gs_addr);
}

static void write_io(uint16_t addr, uint16_t data_hi, uint16_t data_lo) {
    const uint32_t gs_addr = ((uint32_t)IO_BASE << 16) | ((uint32_t)addr);

    const uint32_t data = ((uint32_t)data_hi << 16) | ((uint32_t)data_lo);

    return write_gs(gs_addr, data);
}

static uint16_t read_eeprom(uint32_t addr) {
    static const uint32_t EEPROM_BASE = 0x00E00000;

    const uint32_t lo = addr & 1;
    const uint32_t hi = addr & 0x7FFFE;

    const uint32_t gs_addr = (lo << 20) | (hi << 1) | EEPROM_BASE;
    return (uint16_t)(read_gs(gs_addr) >> 16);
}

static void write_eeprom(uint32_t addr, uint16_t data) {
    static const uint32_t EEPROM_BASE = 0x00E00000;

    const uint32_t lo = addr & 1;
    const uint32_t hi = addr & 0x7FFFE;

    const uint32_t gs_addr = (lo << 20) | (hi << 1) | EEPROM_BASE;
    const uint32_t data_doubled = ((uint32_t)data << 16) | ((uint32_t)data);
    return write_gs(gs_addr, data_doubled);
}

// EEPROM access

#define FIRMWARE_TOO_LARGE (-1)

static void wait_eeprom(void) {
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

static void erase_sst_28lf040(void) {
    // "Chip_Erase"
    write_eeprom(0x0000, 0x3030);
    write_eeprom(0x0000, 0x3030);

    // Tsce
    wait_ms(20);

    wait_eeprom();
}

static void protect_sst_28lf040(void) {
    // "Software_Data_Protect"
    read_eeprom(0x1823);
    read_eeprom(0x1820);
    read_eeprom(0x1822);
    read_eeprom(0x0418);
    read_eeprom(0x041B);
    read_eeprom(0x0419);
    read_eeprom(0x040A);
}

static void unprotect_sst_28lf040(void) {
    // "Software_Data_Unprotect"
    read_eeprom(0x1823);
    read_eeprom(0x1820);
    read_eeprom(0x1822);
    read_eeprom(0x0418);
    read_eeprom(0x041B);
    read_eeprom(0x0419);
    read_eeprom(0x041A);
}

static void erase_sst_29le010(void) {
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

static void protect_sst_29le010(void) {
    // "Software Data Protect Enable & Page Write"
    write_eeprom(0x5555, 0xAAAA);
    write_eeprom(0x2AAA, 0x5555);
    write_eeprom(0x5555, 0xA0A0);

    // important: you must now WAIT 200μs (and ideally call wait_eeprom)
    // before doing anything else except programming data!
}

static void unprotect_sst_29le010(void) {
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

static int write_sst_29le010(void const *data, size_t len) {
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

static int write_sst_28lf040(void const *data, size_t len) {
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

static uint32_t read_ids_alt(void) {
    // "Read_ID"
    write_eeprom(0x0000, 0x9090);

    const uint16_t ids_lo = read_eeprom(0x0000);
    const uint16_t ids_hi = read_eeprom(0x0001);

    // "Reset"
    write_eeprom(0x0000, 0xFFFF);

    return ((uint32_t)ids_lo << 16) | ((uint32_t)ids_hi);
}

static uint32_t read_ids(void) {
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

// PIF shenanigans

// really ought to be a void or something
extern char __nasty_label_hack;

static exception_handler_t old_exception_handler;

static uint32_t sr;

static void exception_handler(exception_t *ex) {
    // pass anything that's not a Watch exception to the libdragon handler
    if (ex->type != EXCEPTION_TYPE_CRITICAL) {
        return old_exception_handler(ex);
    }

    if (ex->code != EXCEPTION_CODE_WATCH) {
        return old_exception_handler(ex);
    }

    // IPL1 clobbers Status, so restore it here
    ex->regs->sr = sr;
    // return to the label defined in assembly
    ex->regs->epc = (uint32_t)&__nasty_label_hack;
    // IPL1 clobbers t1, so set it to an obvious sentinel value that'll show up in the crash handler
    ex->regs->t1 = 0xA5A5A5A55A5A5A5A;
}

int main(void) {
    console_init();
    console_set_render_mode(RENDER_MANUAL);

    console_clear();

    printf("SharkSaver64 v0.0.1\n(C) 2025 Jhynjhiruu, ppcasm\n\n");
    console_render();

    int res = dfs_init(DFS_DEFAULT_LOCATION);
    if (res != DFS_ESUCCESS) {
        printf("Error initialising DFS (%d)\n", res);
        console_render();

        goto __fail;
    }

    char const *path = PATH;

    if (!debug_init_sdfs("sd:/", -1)) {
        printf("No SD card, using firmware from ROM\n");
        console_render();

        path = ROM_PATH;
    }

    const int fw = open(path, O_RDONLY);
    if (fw == -1) {
        printf("Failed to open %s\nErrno: %d (%s)\n", path, errno, strerror(errno));
        console_render();

        goto __fail;
    }

    struct stat sb;
    if (fstat(fw, &sb) == -1) {
        printf("Failed to stat %s\nErrno: %d (%s)\n", path, errno, strerror(errno));
        console_render();

        goto __close_file;
    }

    const off_t size = sb.st_size;
    assertf(size >= 0, "Size is negative\n");
    printf("File %s is 0x%06" PRIXMAX " bytes\n", path, (intmax_t)size);
    console_render();

    uint8_t *const buf = malloc(size);
    if (buf == NULL) {
        printf("Failed to allocate memory\nErrno: %d (%s)\n", errno, strerror(errno));
        console_render();

        goto __close_file;
    }

    size_t done = 0;
    while (done < (size_t)size) {
        const size_t left = (size_t)size - done;
        const ssize_t res = read(fw, buf + done, left);
        if ((res == 0) || (res == -1)) {
            printf("Failed to read %s\nErrno: %d (%s)\n", path, errno, strerror(errno));
            console_render();

            goto __free_pointer;
        }

        done += (size_t)res;
    }

    // install our custom exception handler so we can trap the Watch exception
    old_exception_handler = register_exception_handler(exception_handler);
    // IPL1 clobbers Status, so save it
    sr = C0_STATUS();

    // this isn't actually necessary
    set_RESET_interrupt(true);
    register_RESET_handler(reset_handler_callback);
    // set_RESET_interrupt(false);

    // set the watchpoint for reads (1 << 1) to SP_STATUS
    C0_WRITE_WATCHLO(PhysicalAddr(SP_STATUS) | (1 << 1));

    printf("Press the reset button now\n");
    console_render();

    // very naughty! also probably undefined behaviour but That's Fine™
    // go into a loop, but not in C so that GCC doesn't optimise the rest away,
    //  then put a label here so we can jump here from the exception handler
    //  set t1 as clobbered because IPL1 clobbers it
    //  set cc and memory as clobbered just in case (probably not necessary)
    __asm__ volatile("j .\n"
                     "__nasty_label_hack:\n"
                     :
                     :
                     : "t1", "cc", "memory");

    // disable the watchpoint so we don't get any spurious exceptions later
    C0_WRITE_WATCHLO(0);

    // reinstall libdragon exception handler
    register_exception_handler(old_exception_handler);

    printf("PIF hung! Cart swap to the GameShark now\nPress the A button once the GameShark has been inserted\n");
    console_render();

    joypad_inputs_t inputs = {0}, prev_inputs;
    joypad_init();

    int (*write_firmware)(void const *data, size_t len);

    while (true) {
        while (true) {
            joypad_poll();
            prev_inputs = inputs;
            inputs = joypad_get_inputs(JOYPAD_PORT_1);

            if ((inputs.btn.a) && (!prev_inputs.btn.a)) {
                break;
            }
        }

        const uint32_t ids = read_ids();

        printf("EEPROM IDs: %08" PRIX32 "\n", ids);
        console_render();

        switch (ids) {
            case 0xBFBF0808:
                // SST 29LE010
                write_firmware = &write_sst_29le010;
                printf("EEPROM type: SST 29LE010\n");
                console_render();
                break;

            case 0xBFBF0404:
                // SST 28LF040
                write_firmware = &write_sst_28lf040;
                printf("EEPROM type: SST 28LF040\n");
                console_render();
                break;

            default:
                printf("Unknown EEPROM type\nPress A to rescan\n");
                console_render();
                continue;
        }

        break;
    }

    printf("Writing...\n");
    console_render();

    res = write_firmware(buf, (size_t)size);
    if (res != 0) {
        printf("Error while writing EEPROM (%d)\n", res);
        console_render();

        goto __free_pointer;
    }

    printf("Verifying...\n");
    console_render();

    uint8_t *const verify_buf = malloc(size);
    if (verify_buf == NULL) {
        printf("Failed to allocate memory\nErrno: %d (%s)\n", errno, strerror(errno));
        console_render();

        goto __free_pointer;
    }

    // read the firmware we just wrote
    dma_read(verify_buf, ((uint32_t)GS_BASE << 24) | (0x00C00000), (size_t)size);

    if (memcmp(verify_buf, buf, (size_t)size) != 0) {
        printf("Mismatch between written and expected firmware\n");
        console_render();

        goto __free_verify;
    }

    printf("Success!\n");
    console_render();

__free_verify:
    free(verify_buf);

__free_pointer:
    free(buf);

__close_file:
    if (close(fw) == -1) {
        printf("Failed to close %s\nErrno: %d (%s)\n", path, errno, strerror(errno));
        console_render();
    }

__fail:
    while (true) {
        continue;
    }
}