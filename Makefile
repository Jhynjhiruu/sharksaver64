all: sharksaver64.z64
.PHONY: all
.SECONDARY:

BUILD_DIR = build
SOURCE_DIR = src
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/main.o

N64_CFLAGS += -std=gnu2x -Os -G0

sharksaver64.z64: N64_ROM_TITLE = "SharkSaver64"
sharksaver64.z64: $(BUILD_DIR)/sharksaver64.dfs

$(BUILD_DIR)/sharksaver64.dfs: $(wildcard filesystem/*)

$(BUILD_DIR)/sharksaver64.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)
