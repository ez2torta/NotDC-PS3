#include "dc/aw/aw_flash.h"

#include <stdio.h>
#include <string.h>

#define AW_FLASH_SIZE 0x20000u

enum FlashState
{
	FL_READ,
	FL_UNLOCK1,
	FL_UNLOCK2,
	FL_CMD,
	FL_ERASE0,
	FL_ERASE1,
	FL_AUTOSELECT
};

static u8 s_data[AW_FLASH_SIZE];
static char s_path[256];
static FlashState s_state;
static bool s_dirty;
static unsigned s_busy;

static bool unlock_addr(u32 addr)
{
	return (addr & 0x7FFu) == 0x555u;
}

static bool unlock2_addr(u32 addr)
{
	return (addr & 0x7FFu) == 0x2AAu;
}

static void sector_of(u32 addr, u32* start, u32* size)
{
	if (addr < 0x10000u) { *start = 0x00000u; *size = 0x10000u; return; }
	if (addr < 0x18000u) { *start = 0x10000u; *size = 0x08000u; return; }
	if (addr < 0x1A000u) { *start = 0x18000u; *size = 0x02000u; return; }
	if (addr < 0x1C000u) { *start = 0x1A000u; *size = 0x02000u; return; }
	*start = 0x1C000u;
	*size = 0x04000u;
}

static void erase_sector(u32 addr)
{
	u32 start, size;
	sector_of(addr & (AW_FLASH_SIZE - 1u), &start, &size);
	if (start == 0x1A000u)   // factory-protected sector
		return;
	memset(s_data + start, 0xFF, size);
	s_dirty = true;
	s_busy = 3;
}

static u8 read8(u32 addr)
{
	addr &= AW_FLASH_SIZE - 1u;
	if (s_busy)
	{
		s_busy--;
		return 0x00;   // DQ7 still busy, game keeps polling
	}
	if (s_state == FL_AUTOSELECT)
	{
		if (addr == 0) return 0x04;
		if (addr == 1) return 0xB0;
	}
	return s_data[addr];
}

static void write8(u32 addr, u8 value)
{
	addr &= AW_FLASH_SIZE - 1u;

	switch (s_state)
	{
	case FL_UNLOCK1:
		s_state = (unlock_addr(addr) && value == 0xAA) ? FL_UNLOCK2 : FL_READ;
		return;
	case FL_UNLOCK2:
		s_state = (unlock2_addr(addr) && value == 0x55) ? FL_CMD : FL_READ;
		return;
	case FL_CMD:
		if (unlock_addr(addr) && value == 0xAA)
			s_state = FL_UNLOCK2;
		else if (unlock_addr(addr) && value == 0x80)
			s_state = FL_ERASE0;
		else if (value == 0xA0)
			s_state = FL_ERASE1;
		else if (value == 0x30)
		{
			erase_sector(addr);
			s_state = FL_READ;
		}
		else if (value == 0x90)
			s_state = FL_AUTOSELECT;
		else if (value == 0xF0 || value == 0xFF)
			s_state = FL_READ;
		else
			s_state = FL_READ;
		return;
	case FL_ERASE0:
		if (value == 0x30)
		{
			erase_sector(addr);
			s_state = FL_READ;
		}
		else if (unlock_addr(addr) && value == 0xAA)
			s_state = FL_UNLOCK2;
		else
			s_state = FL_READ;
		return;
	case FL_ERASE1:
		s_data[addr] &= value;
		s_dirty = true;
		s_state = FL_READ;
		return;
	case FL_AUTOSELECT:
		if (value == 0xF0)
			s_state = FL_READ;
		return;
	default:
		if (unlock_addr(addr) && value == 0xAA)
			s_state = FL_UNLOCK2;
		return;
	}
}

bool aw_flash_open(const char* path)
{
	memset(s_data, 0xFF, sizeof(s_data));
	s_state = FL_READ;
	s_dirty = false;
	s_busy = 0;
	s_path[0] = 0;
	if (path)
	{
		strncpy(s_path, path, sizeof(s_path) - 1);
		s_path[sizeof(s_path) - 1] = 0;
		FILE* f = fopen(s_path, "rb");
		if (f)
		{
			size_t n = fread(s_data, 1, sizeof(s_data), f);
			fclose(f);
			if (n != sizeof(s_data))
			{
				printf("[awflash] %s size %u, using 0xFF for the rest\n", s_path, (unsigned)n);
				memset(s_data + n, 0xFF, sizeof(s_data) - n);
			}
		}
		else
			printf("[awflash] no %s, starting at 0xFF\n", s_path);
	}
	printf("[awflash] 128 KB at 0xA0000000\n");
	return true;
}

void aw_flash_close()
{
	aw_flash_save();
	s_path[0] = 0;
}

void aw_flash_save()
{
	if (!s_dirty || !s_path[0])
		return;
	FILE* f = fopen(s_path, "wb");
	if (!f)
	{
		printf("[awflash] cannot save %s\n", s_path);
		return;
	}
	fwrite(s_data, 1, sizeof(s_data), f);
	fclose(f);
	s_dirty = false;
}

u32 aw_flash_read(u32 addr, u32 size)
{
	u32 v = 0;
	for (u32 i = 0; i < size; i++)
		v |= (u32)read8(addr + i) << (i * 8u);
	return v;
}

void aw_flash_write(u32 addr, u32 data, u32 size)
{
	// Flycast ignores wide writes here too; the game's command and backup path uses byte stores.
	if (size == 1)
		write8(addr, (u8)data);
}
