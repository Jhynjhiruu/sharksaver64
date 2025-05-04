#include <libdragon.h>

#include "datel.h"

// GameShark interface code

static uint8_t GS_BASE = 0x10;

static const uint8_t IO_BASE = 0x40;

uint32_t gs_addr(uint32_t addr) {
    const uint32_t masked = addr & 0x00FFFFFF;

    return ((uint32_t)GS_BASE << 24) | (masked);
}

uint32_t read_gs(uint32_t addr) {
    const uint32_t pi_addr = gs_addr(addr);

    assertf(io_accessible(pi_addr), "PI address out of range\n");

    return io_read(pi_addr);
}

void write_gs(uint32_t addr, uint32_t data) {
    const uint32_t pi_addr = gs_addr(addr);

    assertf(io_accessible(pi_addr), "PI address out of range\n");

    return io_write(pi_addr, data);
}

uint32_t read_io(uint16_t addr) {
    const uint32_t gs_addr = ((uint32_t)IO_BASE << 16) | ((uint32_t)addr);

    return read_gs(gs_addr);
}

void write_io(uint16_t addr, uint16_t data_hi, uint16_t data_lo) {
    const uint32_t gs_addr = ((uint32_t)IO_BASE << 16) | ((uint32_t)addr);

    const uint32_t data = ((uint32_t)data_hi << 16) | ((uint32_t)data_lo);

    return write_gs(gs_addr, data);
}

uint16_t read_eeprom(uint32_t addr) {
    static const uint32_t EEPROM_BASE = 0x00E00000;

    const uint32_t lo = addr & 1;
    const uint32_t hi = addr & 0x7FFFE;

    const uint32_t gs_addr = (lo << 20) | (hi << 1) | EEPROM_BASE;
    return (uint16_t)(read_gs(gs_addr) >> 16);
}

void write_eeprom(uint32_t addr, uint16_t data) {
    static const uint32_t EEPROM_BASE = 0x00E00000;

    const uint32_t lo = addr & 1;
    const uint32_t hi = addr & 0x7FFFE;

    const uint32_t gs_addr = (lo << 20) | (hi << 1) | EEPROM_BASE;
    const uint32_t data_doubled = ((uint32_t)data << 16) | ((uint32_t)data);
    return write_gs(gs_addr, data_doubled);
}
