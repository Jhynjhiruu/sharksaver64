# SharkSaver64

A tool to flash GameShark & related device firmwares using only a flashcart and your N64.

### Usage

Run the ROM with your favourite flashcart. If there's a file called `fw.bin` on the root of the SD card, it'll use that, otherwise it'll use the compiled-in firmware - by default, GameShark Pro v3.30 (April). Then, just follow the instructions on the screen.

### Building

You'll need [libdragon](https://github.com/DragonMinded/libdragon/) installed, and make sure you're on the `preview` branch.

Find the function `exception_reset_time` in `src/interrupt.c`, and edit it to just return 0. Build and install.

Then, just run `make` here to build the ROM.

### Credits

 - Implementation: [@Jhynjhiruu](https://github.com/Jhynjhiruu/)
 - Idea & expertise: [@ppcasm](https://github.com/ppcasm/)
 - Support: [@Modman](https://github.com/RWeick/)
