#pragma once

#include <libdragon.h>

#define FIRMWARE_TOO_LARGE (-1)

void wait_eeprom(void);

void erase_sst_28lf040(void);
void protect_sst_28lf040(void);
void unprotect_sst_28lf040(void);
int write_sst_28lf040(void const *data, size_t len);

void erase_sst_29le010(void);
void protect_sst_29le010(void);
void unprotect_sst_29le010(void);
int write_sst_29le010(void const *data, size_t len);

uint32_t read_ids_alt(void);

uint32_t read_ids(void);
