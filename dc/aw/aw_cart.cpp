#include "dc/aw/aw_cart.h"
#include "dc/aw/aw_rom.h"

#include <stdio.h>
#include <string.h>

#define AW_CART_BASE 0x005F7000u

enum AwMode { AW_EPR = 0, AW_MPR_RECORD = 1, AW_MPR_FILE = 2 };

static u32 s_mpr_offset;
static u32 s_epr_offset, s_mpr_bank, s_mpr_file_offset;
static u16 s_mpr_record_index, s_mpr_first_file_index;
static u32 s_dma_offset, s_dma_limit;
static AwMode s_mode;
static u32 s_dma_starts, s_dma_bytes, s_dma_past_end;

static u32 word(u32 index)
{
	const u32 words = aw_rom_size() / 2u;
	if (words == 0)
		return 0;
	return aw_rom_read16((index % words) * 2u);
}

static void recalc(AwMode mode)
{
	s_mode = mode;
	switch (mode)
	{
	case AW_EPR:
		s_dma_offset = s_epr_offset * 2u;
		s_dma_limit = s_mpr_offset;
		break;
	case AW_MPR_RECORD:
		s_dma_offset = s_mpr_offset + (u32)s_mpr_record_index * 0x40u;
		s_dma_limit = 0x08000000u;
		break;
	case AW_MPR_FILE:
	{
		const u32 rec = (s_mpr_bank * 0x08000000u + s_mpr_offset +
		                 (u32)s_mpr_first_file_index * 0x40u + 8u) / 2u;
		u32 abs = word(rec) | (word(rec + 1u) << 16);
		s_dma_offset = (s_mpr_offset + abs + s_mpr_file_offset * 2u) & 0x07FFFFFFu;
		s_dma_limit = 0x08000000u;
		break;
	}
	}
	if (s_dma_offset >= s_mpr_offset)
	{
		const u32 bank = s_mpr_bank * 0x08000000u;
		s_dma_offset += bank;
		s_dma_limit += bank;
	}
	if (s_dma_limit > aw_rom_size())
		s_dma_limit = aw_rom_size();
}

void aw_cart_init(u32 mpr_offset)
{
	s_mpr_offset = mpr_offset;
	aw_cart_reset();
	printf("[awcart] regs 5F7000, mpr_offset=%08X\n", mpr_offset);
}

void aw_cart_reset()
{
	s_epr_offset = 0;
	s_mpr_bank = 0;
	s_mpr_file_offset = 0;
	s_mpr_record_index = 0;
	s_mpr_first_file_index = 0;
	s_dma_offset = 0;
	s_dma_limit = 0;
	s_mode = AW_EPR;
	s_dma_starts = s_dma_bytes = s_dma_past_end = 0;
}

u32 aw_cart_read(u32 addr, u32 size)
{
	const u32 off = addr - AW_CART_BASE;
	u32 v = 0xFFFFu;
	switch (off)
	{
	case 0x00: v = s_epr_offset & 0xFFFFu; break;
	case 0x04: v = (s_epr_offset >> 16) & 0xFFFFu; break;
	case 0x0C: v = s_mpr_record_index; break;
	case 0x10: v = s_mpr_first_file_index; break;
	case 0x14: v = s_mpr_file_offset & 0xFFFFu; break;
	case 0x18: v = (s_mpr_file_offset >> 16) & 0xFFFFu; break;
	case 0x80:
	{
		u32 roffset = s_epr_offset & 0x3FFFFFFu;
		if (roffset >= s_mpr_offset / 2u)
			roffset += s_mpr_bank * 0x4000000u;
		v = roffset * 2u < aw_rom_size() ? aw_rom_read16(roffset * 2u) : 0u;
		break;
	}
	default: break;
	}
	if (size == 1)
		return v & 0xFFu;
	if (size == 2)
		return v & 0xFFFFu;
	return v;
}

void aw_cart_write(u32 addr, u32 data, u32 size)
{
	const u32 off = addr - AW_CART_BASE;
	(void)size;
	switch (off)
	{
	case 0x00:
		s_epr_offset = (s_epr_offset & 0xFFFF0000u) | (data & 0xFFFFu);
		recalc(AW_EPR);
		break;
	case 0x04:
		s_epr_offset = (s_epr_offset & 0x0000FFFFu) | ((data & 0xFFFFu) << 16);
		recalc(AW_EPR);
		break;
	case 0x0C:
		s_mpr_record_index = (u16)data;
		recalc(AW_MPR_RECORD);
		break;
	case 0x10:
		s_mpr_first_file_index = (u16)data;
		recalc(AW_MPR_FILE);
		break;
	case 0x14:
		s_mpr_file_offset = (s_mpr_file_offset & 0xFFFF0000u) | (data & 0xFFFFu);
		recalc(AW_MPR_FILE);
		break;
	case 0x18:
		s_mpr_file_offset = (s_mpr_file_offset & 0x0000FFFFu) | ((data & 0xFFFFu) << 16);
		recalc(AW_MPR_FILE);
		break;
	case 0x80:
		if (s_epr_offset == 0x007FFFFFu)
			s_mpr_bank = data & 3u;
		break;
	default:
		break;
	}
}

void aw_cart_dma_begin(u32 dest, u32 len)
{
	(void)dest;
	(void)len;
	s_dma_starts++;
}

u32 aw_cart_dma_read(u8* dst, u32 want)
{
	const u32 room = s_dma_offset < s_dma_limit ? s_dma_limit - s_dma_offset : 0;
	u32 size = want;
	if (size > 32u)
		size = 32u;
	if (size > room)
		size = room;
	size &= ~1u;
	if (size == 0)
	{
		s_dma_past_end++;
		return 0;
	}
	u32 got = aw_rom_read(s_dma_offset, dst, size);
	s_dma_offset += got;
	s_dma_bytes += got;
	return got;
}

u32 aw_cart_dma_offset()
{
	return s_dma_offset;
}

u32 aw_cart_mpr_offset()
{
	return s_mpr_offset;
}
