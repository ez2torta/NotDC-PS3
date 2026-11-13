#pragma once
#include "types.h"

// 128 KB Atomiswave BIOS flash, visible at physical 0x00000 and at 0xA0000000.
// Only the AMD command subset the game uses is implemented; persistence is aw_flash.bin.

bool aw_flash_open(const char* path);
void aw_flash_close();
void aw_flash_save();
u32  aw_flash_read(u32 addr, u32 size);
void aw_flash_write(u32 addr, u32 data, u32 size);
