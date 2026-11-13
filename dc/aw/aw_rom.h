#pragma once
#include "types.h"

// Virtual Atomiswave ROM: EPR (fmem1) plus the MPR filesystem described by fs_extracted/_manifest.txt.
// The guest addresses are the Flycast "RomPtr" layout; read16() converts them to loose files.

bool aw_rom_open(const char* fmem1_path, const char* fs_dir,
                 const char* records0, const char* records1, const char* records2,
                 u32* out_mpr_offset, u32* out_entry);
bool aw_rom_open_packed(const char* fmem1_path, const char* rom_dir,
                        u32* out_mpr_offset, u32* out_entry);
void aw_rom_close();
u16  aw_rom_read16(u32 byte_off);
u32  aw_rom_read(u32 byte_off, u8* dst, u32 len);
u32  aw_rom_size();
