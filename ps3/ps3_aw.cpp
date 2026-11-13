#include "ps3/ps3_aw.h"

#include "dc/aw/aw_cart.h"
#include "dc/aw/aw_board.h"
#include "dc/aw/aw_flash.h"
#include "dc/aw/aw_rom.h"
#include "dc/mem/sh4_mem.h"
#include "dc/pvr/pvr_if.h"
#include "dc/sh4/sh4_registers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>

bool g_aw_active;
bool g_aw_pending;

static char s_fmem1[512];
static char s_fs[512];
static char s_records[3][512];
static char s_rom_dir[512];
static char s_flash[512];
static bool s_packed;

void aw_trace(const char* fmt, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	FILE* f = fopen("/dev_hdd0/data/NotDC-AW/aw_trace.log", "a");
	if (f)
	{
		fputs(buf, f);
		fputc('\n', f);
		fclose(f);
	}
}

static bool ends_with_aw(const char* path)
{
	const size_t n = path ? strlen(path) : 0;
	return n >= 3 && path[n - 3] == '.' &&
	       (path[n - 2] == 'a' || path[n - 2] == 'A') &&
	       (path[n - 1] == 'w' || path[n - 1] == 'W');
}

static char* trim(char* s)
{
	while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
		s++;
	char* e = s + strlen(s);
	while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
		*--e = 0;
	return s;
}

static void set_defaults(const char* descriptor, char* dir, size_t dir_size)
{
	strncpy(dir, descriptor, dir_size - 1);
	dir[dir_size - 1] = 0;
	char* slash = strrchr(dir, '/');
	if (slash)
		*slash = 0;
	else
		dir[0] = 0;

	if (dir[0])
	{
		snprintf(s_fmem1, sizeof(s_fmem1), "%s/ax3201p01.fmem1.dec", dir);
		snprintf(s_fs, sizeof(s_fs), "%s/fs_extracted", dir);
		snprintf(s_records[0], sizeof(s_records[0]), "%s/records0.bin", dir);
		snprintf(s_records[1], sizeof(s_records[1]), "%s/records1.bin", dir);
		snprintf(s_records[2], sizeof(s_records[2]), "%s/records2.bin", dir);
		snprintf(s_rom_dir, sizeof(s_rom_dir), "%s/roms", dir);
		snprintf(s_flash, sizeof(s_flash), "%s/aw_flash.bin", dir);
	}
	else
	{
		snprintf(s_fmem1, sizeof(s_fmem1), "ax3201p01.fmem1.dec");
		snprintf(s_fs, sizeof(s_fs), "fs_extracted");
		snprintf(s_records[0], sizeof(s_records[0]), "records0.bin");
		snprintf(s_records[1], sizeof(s_records[1]), "records1.bin");
		snprintf(s_records[2], sizeof(s_records[2]), "records2.bin");
		snprintf(s_rom_dir, sizeof(s_rom_dir), "roms");
		snprintf(s_flash, sizeof(s_flash), "aw_flash.bin");
	}
}

static void resolve(const char* dir, const char* value, char* out, size_t out_size)
{
	if (value[0] == '/')
		snprintf(out, out_size, "%s", value);
	else if (dir[0])
		snprintf(out, out_size, "%s/%s", dir, value);
	else
		snprintf(out, out_size, "%s", value);
}

bool aw_descriptor_path(const char* path)
{
	return ends_with_aw(path);
}

bool aw_prepare(const char* descriptor)
{
	aw_trace("[aw] prepare %s", descriptor);
	FILE* f = fopen(descriptor, "rb");
	if (!f)
	{
		printf("[aw] cannot open descriptor %s\n", descriptor);
		aw_trace("[aw] cannot open descriptor");
		return false;
	}

	char dir[512];
	set_defaults(descriptor, dir, sizeof(dir));
	s_packed = false;

	char line[768];
	while (fgets(line, sizeof(line), f))
	{
		char* t = trim(line);
		if (!t[0] || t[0] == '#')
			continue;
		char* eq = strchr(t, '=');
		if (!eq)
			continue;
		*eq = 0;
		char* key = trim(t);
		char* value = trim(eq + 1);
		if (!key[0] || !value[0])
			continue;
		if (strcmp(key, "fmem1") == 0)
			resolve(dir, value, s_fmem1, sizeof(s_fmem1));
		else if (strcmp(key, "fs") == 0)
			resolve(dir, value, s_fs, sizeof(s_fs));
		else if (strcmp(key, "records") == 0 || strcmp(key, "records0") == 0)
			resolve(dir, value, s_records[0], sizeof(s_records[0]));
		else if (strcmp(key, "records1") == 0)
			resolve(dir, value, s_records[1], sizeof(s_records[1]));
		else if (strcmp(key, "records2") == 0)
			resolve(dir, value, s_records[2], sizeof(s_records[2]));
		else if (strcmp(key, "rom_dir") == 0)
			resolve(dir, value, s_rom_dir, sizeof(s_rom_dir));
		else if (strcmp(key, "mode") == 0)
			s_packed = (strcmp(value, "packed") == 0);
		else if (strcmp(key, "flash") == 0)
			resolve(dir, value, s_flash, sizeof(s_flash));
	}
	fclose(f);

	FILE* epr = fopen(s_fmem1, "rb");
	if (!epr)
	{
		printf("[aw] missing fmem1: %s\n", s_fmem1);
		aw_trace("[aw] missing fmem1 %s", s_fmem1);
		return false;
	}
	fclose(epr);

	if (s_packed)
	{
		char rom1[768];
		snprintf(rom1, sizeof(rom1), "%s/ax3201m01.mrom1.dec", s_rom_dir);
		FILE* rf = fopen(rom1, "rb");
		if (!rf)
		{
			printf("[aw] missing packed ROM: %s\n", rom1);
			return false;
		}
		fclose(rf);
	}
	else
	{
		char manifest[768];
		snprintf(manifest, sizeof(manifest), "%s/_manifest.txt", s_fs);
		FILE* mf = fopen(manifest, "rb");
		if (!mf)
		{
			printf("[aw] missing manifest: %s\n", manifest);
			return false;
		}
		fclose(mf);
	}

	g_aw_pending = true;
	aw_trace("[aw] ready mode=%s", s_packed ? "packed" : "fs");
	printf("[aw] descriptor %s\n[aw] mode %s\n[aw] fmem1 %s\n[aw] fs %s\n[aw] rom_dir %s\n"
	       "[aw] records %s %s %s\n[aw] flash %s\n",
	       descriptor, s_packed ? "packed" : "fs", s_fmem1, s_fs, s_rom_dir,
	       s_records[0], s_records[1], s_records[2], s_flash);
	return true;
}

static void copy_to_ram(u32 dst, const u8* src, u32 size)
{
	u8* ram = mem_b.data;
	u32 i = 0;
	for (; i + 4 <= size; i += 4)
	{
		u32 w = 0;
		memcpy(&w, src + i, 4);
		*(u32*)&ram[dst + i] = __builtin_bswap32(w);
	}
	for (; i < size; i++)
		ram[(dst + i) ^ 3] = src[i];
}

static void ensure_flash_dir()
{
	// The full PKG keeps fmem1/fs in USRDIR; only the writable flash lives in the data root.
	mkdir("/dev_hdd0/data/NotDC-AW", 0777);
	mkdir("/dev_hdd0/data/NotDC-AW/aw", 0777);
	mkdir("/dev_hdd0/data/NotDC-AW/aw/kofxi", 0777);
}

bool aw_boot()
{
	aw_trace("[aw] boot enter");
	if (!g_aw_pending)
		return false;
	if (g_aw_active)
		aw_shutdown();

	u32 mpr = 0, entry = 0;
	if (s_packed)
	{
		if (!aw_rom_open_packed(s_fmem1, s_rom_dir, &mpr, &entry))
			return false;
	}
	else if (!aw_rom_open(s_fmem1, s_fs, s_records[0], s_records[1], s_records[2], &mpr, &entry))
		return false;

	FILE* f = fopen(s_fmem1, "rb");
	if (!f)
	{
		printf("[aw] cannot reopen fmem1: %s\n", s_fmem1);
		aw_rom_close();
		return false;
	}
	printf("[aw] loading fmem1\n");
	aw_trace("[aw] loading fmem1");
	u8 buf[64 * 1024];
	u32 dst = 0x0000FF00u;
	u32 total = 0;
	while (total < 0x00800000u)
	{
		u32 want = 0x00800000u - total;
		if (want > sizeof(buf))
			want = sizeof(buf);
		size_t n = fread(buf, 1, want, f);
		if (n == 0)
			break;
		copy_to_ram(dst + total, buf, (u32)n);
		total += (u32)n;
	}
	fclose(f);
	if (total < 0x100u)
	{
		printf("[aw] fmem1 too short: %u bytes\n", total);
		aw_rom_close();
		return false;
	}

	printf("[aw] fmem1 loaded: %u bytes\n", total);
	aw_trace("[aw] fmem1 loaded %u", total);
	ensure_flash_dir();
	aw_trace("[aw] flash+cart");
	aw_flash_open(s_flash);
	aw_cart_init(mpr);
	aw_board_reset();
	pvr_aw_bios_init();

	next_pc = entry;
	r[15] = 0x8CFFF000u;
	sr.SetFull(0x00000000u);
	g_aw_active = true;

	printf("[aw] boot PC=%08X SP=%08X, EPR %u bytes, mpr_offset=%08X\n", entry, r[15], total, mpr);
	aw_trace("[aw] boot PC=%08X", entry);
	return true;
}

void aw_shutdown()
{
	aw_flash_close();
	aw_rom_close();
	g_aw_active = false;
}

void aw_save_flash()
{
	aw_flash_save();
}

extern "C" void ps3_close_game();
void mem_SaveFlash();

bool aw_on_system_reset()
{
	if (!g_aw_active)
		return false;
	aw_trace("[aw] system reset requested by the game: saving flash, back to the menu\n");
	mem_SaveFlash();
	ps3_close_game();
	return true;
}
