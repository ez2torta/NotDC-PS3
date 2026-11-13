// Atomiswave ROM from loose files. EPR is fmem1; MPR is fs_extracted/_manifest.txt.
// The guest uses the Flycast RomPtr layout: fmem1 at 0, mrom1..7 at 0x02000000, 0x04000000,
// 0x06000000, 0x0A000000, 0x0C000000, 0x0E000000 and 0x12000000. The manifest offsets are
// sequential over the concatenated MPR area, so phys_to_concat() converts before a file lookup.

#include "dc/aw/aw_rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AW_ROM_SIZE     0x14000000u
#define AW_EPR_SIZE     0x00800000u
#define AW_RECORD_SIZE  64u
#define AW_RECORD_COUNT 1060u
#define AW_TABLE_SIZE   (AW_RECORD_COUNT * AW_RECORD_SIZE)
#define AW_CACHE_SIZE   4096u

struct AwFile
{
	u32 off;
	u32 size;
	u16 index;
	bool present;
	char name[48];
	char rec_name[48];
};

struct AwRange
{
	u32 off;
	u32 size;
	u16 index;
};

static AwFile  s_files[AW_RECORD_COUNT];
static AwRange s_ranges[AW_RECORD_COUNT];
static u32     s_range_count;
static u8      s_records[3][AW_TABLE_SIZE];
static char    s_fs_dir[256];
static char    s_epr_path[256];

static FILE* s_epr;
static FILE* s_file;
static u16   s_file_index = 0xFFFFu;
static u8    s_file_buf[AW_CACHE_SIZE];
static u32   s_file_buf_off;
static u32   s_file_buf_len;

struct PackSeg
{
	u32 base;
	u32 size;
	FILE* file;
};

static bool s_packed;
static PackSeg s_segs[7];
static char s_rom_dir[256];
static u8 s_pack_buf[64 * 1024];
static int s_pack_seg = -1;
static u32 s_pack_off, s_pack_len;

static u32 le32(const u8* p)
{
	return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static void put32(u8* p, u32 v)
{
	p[0] = (u8)v;
	p[1] = (u8)(v >> 8);
	p[2] = (u8)(v >> 16);
	p[3] = (u8)(v >> 24);
}

static void copy_name(char* dst, const char* src, u32 max)
{
	u32 i = 0;
	for (; i + 1 < max && src[i]; i++)
		dst[i] = src[i];
	dst[i] = 0;
}

static int range_cmp(const void* a, const void* b)
{
	const AwRange* x = (const AwRange*)a;
	const AwRange* y = (const AwRange*)b;
	return x->off < y->off ? -1 : x->off > y->off ? 1 : 0;
}

static u32 bank_base(u32 off)
{
	if (off < 0x06000000u)
		return 0;
	if (off < 0x0C000000u)
		return 0x06000000u;
	return 0x0C000000u;
}

static void build_one_record_table(u8* table, bool normalized)
{
	memset(table, 0xFF, AW_TABLE_SIZE);

	memset(table, 0, AW_RECORD_SIZE);
	memcpy(table, "G1ROM FILESYSTEM", 16);
	put32(table + 16, AW_RECORD_COUNT);

	u8* dot = table + AW_RECORD_SIZE;
	memset(dot, 0, AW_RECORD_SIZE);
	dot[7] = 0x40;
	put32(dot + 8, 0);
	put32(dot + 12, AW_TABLE_SIZE);
	copy_name((char*)dot + 16, ".", 16);

	u8* dotdot = table + AW_RECORD_SIZE * 2;
	memset(dotdot, 0, AW_RECORD_SIZE);
	dotdot[7] = 0x40;
	copy_name((char*)dotdot + 16, "..", 16);

	for (u32 i = 3; i < AW_RECORD_COUNT; i++)
	{
		if (!s_files[i].present)
			continue;
		u8* r = table + i * AW_RECORD_SIZE;
		memset(r, 0, AW_RECORD_SIZE);
		r[7] = 0;
		u32 off = s_files[i].off;
		if (normalized)
			off -= bank_base(off);
		put32(r + 8, off);
		put32(r + 12, s_files[i].size);
		copy_name((char*)r + 16, s_files[i].rec_name, 32);
	}
}

static void build_records(void)
{
	build_one_record_table(s_records[0], false);
	build_one_record_table(s_records[1], true);
	build_one_record_table(s_records[2], true);
}

static bool parse_manifest(void)
{
	char path[512];
	snprintf(path, sizeof(path), "%s/_manifest.txt", s_fs_dir);

	FILE* f = fopen(path, "rb");
	if (!f)
	{
		printf("[awrom] no manifest: %s\n", path);
		return false;
	}

	memset(s_files, 0, sizeof(s_files));
	s_range_count = 0;
	char line[512];
	while (fgets(line, sizeof(line), f))
	{
		if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
			continue;

		int index = 0;
		char original[64] = {0};
		char name[64] = {0};
		unsigned offset = 0, size = 0;
		if (sscanf(line, "%d\t%63[^\t]\t0x%x\t%u\t%63s", &index, original, &offset, &size, name) != 5)
			continue;
		if (index < 3 || index >= (int)AW_RECORD_COUNT || !name[0])
			continue;

		s_files[index].off = offset;
		s_files[index].size = size;
		s_files[index].index = (u16)index;
		s_files[index].present = true;
		copy_name(s_files[index].name, name, sizeof(s_files[index].name));
		copy_name(s_files[index].rec_name, original, sizeof(s_files[index].rec_name));

		AwRange* r = &s_ranges[s_range_count++];
		r->off = offset;
		r->size = size;
		r->index = (u16)index;
	}

	fclose(f);
	qsort(s_ranges, s_range_count, sizeof(s_ranges[0]), range_cmp);
	printf("[awrom] manifest: %u files, %u bytes of record table\n", s_range_count, (unsigned)AW_TABLE_SIZE);
	return true;
}

static bool phys_to_concat(u32 phys, u32* concat)
{
	if (phys >= 0x02000000u && phys < 0x04000000u) { *concat = phys - 0x02000000u; return true; }
	if (phys >= 0x04000000u && phys < 0x06000000u) { *concat = phys - 0x04000000u + 0x02000000u; return true; }
	if (phys >= 0x06000000u && phys < 0x08000000u) { *concat = phys - 0x06000000u + 0x04000000u; return true; }
	if (phys >= 0x0A000000u && phys < 0x0C000000u) { *concat = phys - 0x0A000000u + 0x06000000u; return true; }
	if (phys >= 0x0C000000u && phys < 0x0E000000u) { *concat = phys - 0x0C000000u + 0x08000000u; return true; }
	if (phys >= 0x0E000000u && phys < 0x10000000u) { *concat = phys - 0x0E000000u + 0x0A000000u; return true; }
	if (phys >= 0x12000000u && phys < 0x14000000u) { *concat = phys - 0x12000000u + 0x0C000000u; return true; }
	return false;
}

static const AwRange* find_range(u32 concat)
{
	u32 lo = 0, hi = s_range_count;
	while (lo < hi)
	{
		u32 mid = (lo + hi) / 2;
		const AwRange* r = &s_ranges[mid];
		if (concat < r->off)
			hi = mid;
		else if (concat >= r->off + r->size)
			lo = mid + 1;
		else
			return r;
	}
	return NULL;
}

static u32 read_file(u16 index, u32 off, u8* dst, u32 len)
{
	if (index >= AW_RECORD_COUNT || !s_files[index].present)
		return 0;
	if (off >= s_files[index].size)
		return 0;
	if (len > s_files[index].size - off)
		len = s_files[index].size - off;

	if (s_file_index != index)
	{
		if (s_file)
			fclose(s_file);
		char path[512];
		snprintf(path, sizeof(path), "%s/%s", s_fs_dir, s_files[index].name);
		s_file = fopen(path, "rb");
		s_file_index = s_file ? index : 0xFFFFu;
		s_file_buf_off = 0;
		s_file_buf_len = 0;
	}
	if (!s_file)
		return 0;

	u32 done = 0;
	while (done < len)
	{
		u32 at = off + done;
		if (at < s_file_buf_off || at >= s_file_buf_off + s_file_buf_len)
		{
			s_file_buf_off = at & ~(AW_CACHE_SIZE - 1u);
			if (fseek(s_file, (long)s_file_buf_off, SEEK_SET) != 0)
				break;
			s_file_buf_len = (u32)fread(s_file_buf, 1, sizeof(s_file_buf), s_file);
			if (s_file_buf_len == 0)
				break;
		}
		u32 local = at - s_file_buf_off;
		u32 n = s_file_buf_len - local;
		if (n > len - done)
			n = len - done;
		memcpy(dst + done, s_file_buf + local, n);
		done += n;
	}
	return done;
}

static u32 read_epr(u32 off, u8* dst, u32 len)
{
	if (!s_epr)
		return 0;
	if (off >= AW_EPR_SIZE)
		return 0;
	if (len > AW_EPR_SIZE - off)
		len = AW_EPR_SIZE - off;
	if (fseek(s_epr, (long)off, SEEK_SET) != 0)
		return 0;
	return (u32)fread(dst, 1, len, s_epr);
}

bool aw_rom_open(const char* fmem1_path, const char* fs_dir,
                 const char* records0, const char* records1, const char* records2,
                 u32* out_mpr_offset, u32* out_entry)
{
	aw_rom_close();
	copy_name(s_epr_path, fmem1_path, sizeof(s_epr_path));
	copy_name(s_fs_dir, fs_dir, sizeof(s_fs_dir));

	s_epr = fopen(s_epr_path, "rb");
	if (!s_epr)
	{
		printf("[awrom] cannot open EPR: %s\n", s_epr_path);
		return false;
	}

	u8 hdr[0x100];
	memset(hdr, 0, sizeof(hdr));
	if (fread(hdr, 1, sizeof(hdr), s_epr) != sizeof(hdr))
	{
		printf("[awrom] EPR too short: %s\n", s_epr_path);
		aw_rom_close();
		return false;
	}
	if (memcmp(hdr, "SYSTEM_X_APP", 12) != 0)
	{
		printf("[awrom] EPR is not a decrypted Atomiswave image (no SYSTEM_X_APP)\n");
		aw_rom_close();
		return false;
	}

	u32 mpr = le32(hdr + 0x58);
	u32 entry = le32(hdr + 0x64);
	if (mpr < 0x02000000u || mpr >= AW_ROM_SIZE)
	{
		printf("[awrom] bad mpr_offset %08X, using 02000000\n", mpr);
		mpr = 0x02000000u;
	}
	if ((entry & 0xFF000000u) != 0x8C000000u)
	{
		printf("[awrom] bad gamePC %08X, using 8C010000\n", entry);
		entry = 0x8C010000u;
	}
	*out_mpr_offset = mpr;
	*out_entry = entry;

	if (!parse_manifest())
	{
		aw_rom_close();
		return false;
	}

	build_records();
	const char* paths[3] = {records0, records1, records2};
	for (int bank = 0; bank < 3; bank++)
	{
		if (!paths[bank] || !paths[bank][0])
			continue;
		FILE* rf = fopen(paths[bank], "rb");
		if (!rf)
			continue;
		size_t n = fread(s_records[bank], 1, AW_TABLE_SIZE, rf);
		fclose(rf);
		if (n == AW_TABLE_SIZE)
			printf("[awrom] record table %d: %s\n", bank, paths[bank]);
		else
			printf("[awrom] record table %d %s too short (%u), synthesized\n", bank, paths[bank], (unsigned)n);
	}
	printf("[awrom] EPR %s, MPR %s, mpr_offset=%08X, entry=%08X\n", s_epr_path, s_fs_dir, mpr, entry);
	return true;
}

static bool parse_epr_header(const char* fmem1_path, u32* out_mpr, u32* out_entry)
{
	copy_name(s_epr_path, fmem1_path, sizeof(s_epr_path));
	s_epr = fopen(s_epr_path, "rb");
	if (!s_epr)
	{
		printf("[awrom] cannot open EPR: %s\n", s_epr_path);
		return false;
	}
	u8 hdr[0x100];
	memset(hdr, 0, sizeof(hdr));
	if (fread(hdr, 1, sizeof(hdr), s_epr) != sizeof(hdr))
	{
		printf("[awrom] EPR too short: %s\n", s_epr_path);
		return false;
	}
	if (memcmp(hdr, "SYSTEM_X_APP", 12) != 0)
	{
		printf("[awrom] EPR is not a decrypted Atomiswave image (no SYSTEM_X_APP)\n");
		return false;
	}
	u32 mpr = le32(hdr + 0x58);
	u32 entry = le32(hdr + 0x64);
	if (mpr < 0x02000000u || mpr >= AW_ROM_SIZE)
	{
		printf("[awrom] bad mpr_offset %08X, using 02000000\n", mpr);
		mpr = 0x02000000u;
	}
	if ((entry & 0xFF000000u) != 0x8C000000u)
	{
		printf("[awrom] bad gamePC %08X, using 8C010000\n", entry);
		entry = 0x8C010000u;
	}
	*out_mpr = mpr;
	*out_entry = entry;
	return true;
}

bool aw_rom_open_packed(const char* fmem1_path, const char* rom_dir,
                        u32* out_mpr_offset, u32* out_entry)
{
	static const char* names[7] = {
		"ax3201m01.mrom1.dec", "ax3202m01.mrom2.dec", "ax3203m01.mrom3.dec",
		"ax3204m01.mrom4.dec", "ax3205m01.mrom5.dec", "ax3206m01.mrom6.dec",
		"ax3207m01.mrom7.dec",
	};
	static const u32 bases[7] = {
		0x02000000u, 0x04000000u, 0x06000000u, 0x0A000000u,
		0x0C000000u, 0x0E000000u, 0x12000000u,
	};

	aw_rom_close();
	copy_name(s_rom_dir, rom_dir, sizeof(s_rom_dir));
	u32 mpr = 0, entry = 0;
	if (!parse_epr_header(fmem1_path, &mpr, &entry))
	{
		aw_rom_close();
		return false;
	}

	for (int i = 0; i < 7; i++)
	{
		char path[512];
		snprintf(path, sizeof(path), "%s/%s", s_rom_dir, names[i]);
		s_segs[i].file = fopen(path, "rb");
		s_segs[i].base = bases[i];
		s_segs[i].size = 0x02000000u;
		if (!s_segs[i].file)
		{
			printf("[awrom] missing packed ROM: %s\n", path);
			aw_rom_close();
			return false;
		}
	}

	s_packed = true;
	*out_mpr_offset = mpr;
	*out_entry = entry;
	printf("[awrom] packed EPR %s, ROM dir %s, mpr_offset=%08X, entry=%08X\n",
	       s_epr_path, s_rom_dir, mpr, entry);
	return true;
}

void aw_rom_close()
{
	if (s_epr)
	{
		fclose(s_epr);
		s_epr = NULL;
	}
	if (s_file)
	{
		fclose(s_file);
		s_file = NULL;
	}
	s_file_index = 0xFFFFu;
	s_file_buf_off = s_file_buf_len = 0;
	for (int i = 0; i < 7; i++)
	{
		if (s_segs[i].file)
		{
			fclose(s_segs[i].file);
			s_segs[i].file = NULL;
		}
	}
	s_packed = false;
	s_pack_seg = -1;
	s_pack_off = s_pack_len = 0;
}

u32 aw_rom_size()
{
	return AW_ROM_SIZE;
}

static u32 packed_read(u32 byte_off, u8* dst, u32 len)
{
	byte_off %= AW_ROM_SIZE;
	if (len > AW_ROM_SIZE - byte_off)
		len = AW_ROM_SIZE - byte_off;
	u32 done = 0;
	while (done < len)
	{
		u32 off = byte_off + done;
		u32 n = 0;
		if (off < AW_EPR_SIZE)
		{
			n = read_epr(off, dst + done, len - done);
		}
		else
		{
			for (int i = 0; i < 7; i++)
			{
				if (!s_segs[i].file || off < s_segs[i].base || off >= s_segs[i].base + s_segs[i].size)
					continue;
				const u32 local = off - s_segs[i].base;
				if (s_pack_seg != i || local < s_pack_off || local >= s_pack_off + s_pack_len)
				{
					s_pack_seg = i;
					s_pack_off = local & ~(u32)(sizeof(s_pack_buf) - 1);
					if (fseek(s_segs[i].file, (long)s_pack_off, SEEK_SET) != 0)
					{
						s_pack_len = 0;
						n = 0;
						break;
					}
					s_pack_len = (u32)fread(s_pack_buf, 1, sizeof(s_pack_buf), s_segs[i].file);
					if (s_pack_len == 0)
					{
						n = 0;
						break;
					}
				}
				const u32 in_buf = local - s_pack_off;
				n = s_pack_len - in_buf;
				if (n > len - done)
					n = len - done;
				memcpy(dst + done, s_pack_buf + in_buf, n);
				break;
			}
			if (n == 0)
			{
				// Gap (0x08000000..0x0A000000 and 0x10000000..0x12000000) or short file.
				memset(dst + done, 0, len - done);
				break;
			}
		}
		if (n == 0)
		{
			memset(dst + done, 0, len - done);
			break;
		}
		done += n;
	}
	return done;
}

u32 aw_rom_read(u32 byte_off, u8* dst, u32 len)
{
	if (s_packed)
		return packed_read(byte_off, dst, len);
	byte_off %= AW_ROM_SIZE;
	if (len > AW_ROM_SIZE - byte_off)
		len = AW_ROM_SIZE - byte_off;
	u32 done = 0;
	while (done < len)
	{
		u32 off = byte_off + done;
		u32 n = 0;

		if (off < AW_EPR_SIZE)
		{
			n = read_epr(off, dst + done, len - done);
		}
		else
		{
			// Each 128 MB bank starts with a copy of the G1ROM record table. mrom1/mrom4/mrom7
			// are the three bank starts the game can select with AW_PIO_DATA.
			u32 rec_off = 0xFFFFFFFFu;
			if (off >= 0x02000000u && off < 0x02000000u + AW_TABLE_SIZE) rec_off = off - 0x02000000u;
			else if (off >= 0x0A000000u && off < 0x0A000000u + AW_TABLE_SIZE) rec_off = off - 0x0A000000u;
			else if (off >= 0x12000000u && off < 0x12000000u + AW_TABLE_SIZE) rec_off = off - 0x12000000u;
			if (rec_off != 0xFFFFFFFFu)
			{
				n = AW_TABLE_SIZE - rec_off;
				if (n > len - done)
					n = len - done;
				memcpy(dst + done, s_records[off >= 0x12000000u ? 2 : off >= 0x0A000000u ? 1 : 0] + rec_off, n);
			}
			else
			{
				u32 concat = 0;
				if (!phys_to_concat(off, &concat))
				{
					memset(dst + done, 0, len - done);
					return len;
				}
				if (concat < AW_TABLE_SIZE)
				{
					n = AW_TABLE_SIZE - concat;
					if (n > len - done)
						n = len - done;
					memcpy(dst + done, s_records[0] + concat, n);
				}
				else
				{
					const AwRange* r = find_range(concat);
					if (!r)
					{
						memset(dst + done, 0, len - done);
						return len;
					}
					n = read_file(r->index, concat - r->off, dst + done, len - done);
					if (n == 0)
					{
						memset(dst + done, 0, len - done);
						return len;
					}
				}
			}
		}

		if (n == 0)
		{
			memset(dst + done, 0, len - done);
			break;
		}
		done += n;
	}
	return done;
}

u16 aw_rom_read16(u32 byte_off)
{
	u8 b[2] = {0, 0};
	aw_rom_read(byte_off & ~1u, b, 2);
	return (u16)b[0] | ((u16)b[1] << 8);
}
