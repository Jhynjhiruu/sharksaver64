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

static const char PATH[] = "sd:/fw.bin";
static const char ROM_PATH[] = "rom:/fw.bin";

int main(void) {
    console_init();
    console_set_render_mode(RENDER_MANUAL);

    console_clear();

    printf("SharkSaver64 v0.0.2\n(C) 2025 Jhynjhiruu, ppcasm\n\n");
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

    void reset_handler_callback(void) {
        printf("Reset button pressed, waiting for exception...\n");
        console_render();
    }

    void setup_callback(void) {
        printf("Press the reset button now\n");
        console_render();
    }

    hang_pif(reset_handler_callback, setup_callback);

    printf("Swap to the GameShark now, then press the A button to flash!\n");
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
    dma_read(verify_buf, gs_addr(0x00C00000), (size_t)size);

    for (size_t i = 0; i < (size_t)size; i++) {
        if (verify_buf[i] != buf[i]) {
            printf("Mismatch between written and expected firmware at 0x%06" PRIXMAX " (got %02" PRIX8 ", expected %02" PRIX8 ")\n", (uintmax_t)i, verify_buf[i], buf[i]);
            console_render();

            goto __free_verify;
        }
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
