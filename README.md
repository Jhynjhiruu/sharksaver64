# SharkSaver64

A tool to flash GameShark & related device firmwares using only a flashcart and your N64.

### Usage

Run the ROM with your favourite flashcart (if using a PicoCart64, flash the `.uf2` file to it). If your card supports SD card access and there's a file called `fw.bin` on the root, it'll use that, otherwise it'll use the compiled-in firmware - by default, GameShark Pro v3.30 (April). Then, just follow the instructions on the screen.

Note: encrypted firmware updates (e.g. `ar3.enc`) are not currently supported. Decrypt them first before flashing.

Compatible dumps of most GameShark firmwares are available from [the SharkDumps repository](https://github.com/LibreShark/sharkdumps/blob/main/n64-gameshark.md#firmware).

For example, to flash the `v1.01` firmware, download [`gs-1.01-19970731-pristine.bin`](https://github.com/LibreShark/sharkdumps/blob/main/n64/firmware/gs-1.01-19970731-Pristine.bin), rename it to `fw.bin`, and either:
1. place it on the root of your flashcart's SD card, then run the ROM, or
2. place it in the `filesystem/` directory of this repository (overwriting the existing file), [build](#building) the ROM with your installed `libdragon` toolchain, and run the built ROM

### Building

You'll need [libdragon](https://github.com/DragonMinded/libdragon/) installed, and make sure you're on the `preview` branch.

Find the function `exception_reset_time` in `src/interrupt.c`, and edit it to just return 0. Build and install.

Then, just run `make` here to build the ROM.

### Credits

 - Implementation: [@Jhynjhiruu](https://github.com/Jhynjhiruu/)
 - Idea & expertise: [@ppcasm](https://github.com/ppcasm/)
 - Support: [@Modman](https://github.com/RWeick/)
