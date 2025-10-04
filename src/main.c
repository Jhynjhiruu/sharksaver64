#include <libcart/cart.h>
#include <libdragon.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <malloc.h>
#include <sys/stat.h>
#include <unistd.h>

#include "datel.h"
#include "eeprom.h"
#include "pif.h"

static const char SD_PATH[] = "sd:/fw.bin";
static const char ROM_PATH[] = "rom:/fw.bin";

joypad_inputs_t inputs = {0}, prev_inputs;
#define INPUT(inp) ((inputs.inp) && !(prev_inputs.inp))

void print_header(void) {
    console_clear();
    printf("SharkSaver64 v0.0.3\n(C) 2025 Jhynjhiruu, ppcasm\n\n");
}

void swap(char *to) {
    void reset_handler_callback(void) {
        printf("Reset button pressed, waiting for exception...\n");
        console_render();
    }

    void setup_callback(void) {
        printf("Press the reset button now\n");
        console_render();
    }

    hang_pif(reset_handler_callback, setup_callback);

    printf("Swap to the %s now, then press the A button to flash!\n", to);
    console_render();
}

void write_fw(const char *path) {
    print_header();

    const int fw = open(path, O_RDONLY);
    if (fw == -1) {
        printf("Failed to open %s\nErrno: %d (%s)\n", path, errno, strerror(errno));
        console_render();

        goto __write_fw__fail;
    }

    struct stat sb;
    if (fstat(fw, &sb) == -1) {
        printf("Failed to stat %s\nErrno: %d (%s)\n", path, errno, strerror(errno));
        console_render();

        goto __write_fw__close_file;
    }

    const off_t size = sb.st_size;
    assertf(size >= 0, "Size is negative\n");
    printf("File %s is 0x%06" PRIXMAX " bytes\n", path, (intmax_t)size);
    console_render();

    uint8_t *const buf = malloc(size);
    if (buf == NULL) {
        printf("Failed to allocate memory\nErrno: %d (%s)\n", errno, strerror(errno));
        console_render();

        goto __write_fw__close_file;
    }

    size_t done = 0;
    while (done < (size_t)size) {
        const size_t left = (size_t)size - done;
        const ssize_t res = read(fw, buf + done, left);
        if ((res == 0) || (res == -1)) {
            printf("Failed to read %s\nErrno: %d (%s)\n", path, errno, strerror(errno));
            console_render();

            goto __write_fw__free_pointer;
        }

        done += (size_t)res;
    }

    // should really call debug_close_sdfs() here, but it doesn't matter much

    swap("GameShark");

    int (*write_firmware)(void const *data, size_t len);

    while (true) {
        while (true) {
            joypad_poll();
            prev_inputs = inputs;
            inputs = joypad_get_inputs(JOYPAD_PORT_1);

            if (INPUT(btn.a)) {
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

    int res = write_firmware(buf, (size_t)size);
    if (res != 0) {
        printf("Error while writing EEPROM (%d)\n", res);
        console_render();

        goto __write_fw__free_pointer;
    }

    printf("Verifying...\n");
    console_render();

    uint8_t *const verify_buf = malloc(size);
    if (verify_buf == NULL) {
        printf("Failed to allocate memory\nErrno: %d (%s)\n", errno, strerror(errno));
        console_render();

        goto __write_fw__free_pointer;
    }

    // read the firmware we just wrote
    dma_read(verify_buf, gs_addr(0x00C00000), (size_t)size);

    for (size_t i = 0; i < (size_t)size; i++) {
        if (verify_buf[i] != buf[i]) {
            printf("Mismatch between written and expected firmware at 0x%06" PRIXMAX " (got %02" PRIX8 ", expected %02" PRIX8 ")\n", (uintmax_t)i, verify_buf[i], buf[i]);
            console_render();

            goto __write_fw__free_verify;
        }
    }

    printf("Success!\n");
    console_render();

__write_fw__free_verify:
    free(verify_buf);

__write_fw__free_pointer:
    free(buf);

__write_fw__close_file:
    if (close(fw) == -1) {
        printf("Failed to close %s\nErrno: %d (%s)\n", path, errno, strerror(errno));
        console_render();
    }

__write_fw__fail:
}

void read_fw(const char *path) {
    swap("GameShark");

    size_t size;

    while (true) {
        while (true) {
            joypad_poll();
            prev_inputs = inputs;
            inputs = joypad_get_inputs(JOYPAD_PORT_1);

            if (INPUT(btn.a)) {
                while (INPUT(btn.a)) {
                    continue;
                }
                break;
            }
        }

        const uint32_t ids = read_ids();

        printf("EEPROM IDs: %08" PRIX32 "\n", ids);
        console_render();

        switch (ids) {
            case 0xBFBF0808:
                // SST 29LE010
                size = 0x40000;
                printf("EEPROM type: SST 29LE010\n");
                console_render();
                break;

            case 0xBFBF0404:
                // SST 28LF040
                size = 0x100000;
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

    printf("Reading...\n");
    console_render();

    uint8_t *const read_buf = malloc(size);
    if (read_buf == NULL) {
        printf("Failed to allocate memory\nErrno: %d (%s)\n", errno, strerror(errno));
        console_render();

        goto __read_fw__fail;
    }

    // read the firmware
    dma_read(read_buf, gs_addr(0x00C00000), size);

    printf("Swap to the flashcart now, then press the A button to save the dump!\n");
    console_render();

    while (true) {
        while (true) {
            joypad_poll();
            prev_inputs = inputs;
            inputs = joypad_get_inputs(JOYPAD_PORT_1);

            if (INPUT(btn.a)) {
                while (INPUT(btn.a)) {
                    continue;
                }
                break;
            }
        }

        if (cart_init() >= 0) {
            break;
        }

        printf("Failed to initialise flashcart\nPress A to rescan\n");
        console_render();
    }

    const int fw = creat(path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (fw == -1) {
        printf("Failed to create %s\nErrno: %d (%s)\n", path, errno, strerror(errno));
        console_render();

        goto __read_fw__free_pointer;
    }

    size_t done = 0;
    while (done < size) {
        const size_t left = size - done;
        const ssize_t res = write(fw, read_buf + done, left);
        if ((res == 0) || (res == -1)) {
            printf("Failed to write %s\nErrno: %d (%s)\n", path, errno, strerror(errno));
            console_render();

            goto __read_fw__close_file;
        }

        done += (size_t)res;
    }

    printf("Success!\n");
    console_render();

__read_fw__close_file:
    if (close(fw) == -1) {
        printf("Failed to close %s\nErrno: %d (%s)\n", path, errno, strerror(errno));
        console_render();
    }

__read_fw__free_pointer:
    free(read_buf);

__read_fw__fail:
}

int main(void) {
    console_init();
    console_set_render_mode(RENDER_MANUAL);

    if (sys_bbplayer()) {
        printf("This program cannot be used on an iQue Player!\n");
        console_render();

        while (true) {
            continue;
        }
    }

    joypad_init();

    bool has_dfs = false;

    int res = dfs_init(DFS_DEFAULT_LOCATION);
    if (res == DFS_ESUCCESS) {
        has_dfs = true;
    }

    bool has_sd = false;

    if (debug_init_sdfs("sd:/", -1)) {
        has_sd = true;
    }

    const char *options[3];
    enum {
        WRITE_SD,
        WRITE_DFS,
        READ_SD,
    } modes[3];
    uint num_options = 0;

    if (has_sd) {
        options[num_options] = "Write fw.bin from SD card to GameShark";
        modes[num_options] = WRITE_SD;
        num_options++;
    }

    if (has_dfs) {
        options[num_options] = "Write embedded fw.bin to GameShark";
        modes[num_options] = WRITE_DFS;
        num_options++;
    }

    if (/*has_sd*/true) {
        options[num_options] = "Dump fw.bin from GameShark to SD card";
        modes[num_options] = READ_SD;
        num_options++;
    }

    uint cursor_pos = 0;

    while (true) {
        joypad_poll();
        print_header();

        for (uint i = 0; i < num_options; i++) {
            printf("%c %s\n", (i == cursor_pos) ? '>' : ' ', options[i]);
        }

        printf("\nPress A to select an option\n");

        console_render();

        prev_inputs = inputs;
        inputs = joypad_get_inputs(JOYPAD_PORT_1);

        if ((INPUT(btn.d_down) || INPUT(stick_y <= -40)) && (cursor_pos < (num_options - 1))) {
            cursor_pos++;
        } else if ((INPUT(btn.d_up) || INPUT(stick_y >= 40)) && (cursor_pos > 0)) {
            cursor_pos--;
        } else if (INPUT(btn.a == false)) {
            break;
        }
    }

    switch (modes[cursor_pos]) {
        case WRITE_SD:
            write_fw(SD_PATH);
            break;

        case WRITE_DFS:
            write_fw(ROM_PATH);
            break;

        case READ_SD:
            read_fw(SD_PATH);
            break;
    }

    while (true) {
        continue;
    }
}
