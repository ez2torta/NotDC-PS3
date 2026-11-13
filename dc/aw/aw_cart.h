#pragma once
#include "types.h"

// Atomiswave cartridge registers (0x5F7000) and the G1 DMA source. The formulas are the
// Flycast AWCartridge ones already documented in docs/research/2026-09-29-fase1-cartucho-aw.md.

void aw_cart_init(u32 mpr_offset);
void aw_cart_reset();
u32  aw_cart_read(u32 addr, u32 size);
void aw_cart_write(u32 addr, u32 data, u32 size);
void aw_cart_dma_begin(u32 dest, u32 len);
u32  aw_cart_dma_read(u8* dst, u32 want);
u32  aw_cart_dma_offset();
u32  aw_cart_mpr_offset();
