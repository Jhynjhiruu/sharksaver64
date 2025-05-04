#pragma once

#include <libdragon.h>

uint32_t gs_addr(uint32_t addr);

uint32_t read_gs(uint32_t addr);
void write_gs(uint32_t addr, uint32_t data);

uint32_t read_io(uint16_t addr);
void write_io(uint16_t addr, uint16_t data_hi, uint16_t data_lo);

uint16_t read_eeprom(uint32_t addr);
void write_eeprom(uint32_t addr, uint16_t data);
