#pragma once
#include "types.h"

// Atomiswave board I/O at area 0 0x600000..0x6007FF (repeats every 0x800).
// F2 implements the register surface; coin/pad input is wired in F3.

u32  aw_board_read(u32 addr, u32 size);
void aw_board_write(u32 addr, u32 data, u32 size);
void aw_board_reset();
