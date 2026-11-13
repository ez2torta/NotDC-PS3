#include "ps3/ps3_aw_cheats.h"

#include "dc/mem/sh4_mem.h"
#include "ps3/ps3_aw.h"

int g_aw_freeplay;
int g_aw_all_bosses;
int g_aw_same_chars;
int g_aw_ldm_any;

// Offsets from the KOF XI AW executable; the recomp cheat module uses the same addresses.
static const u32 kDbgFlags1 = 0x8C35AB3Cu;
static const u32 kDbgFlags2 = 0x8C35AB40u;
static const u32 kUnlockMask = 0x8C18F09Au;

void aw_cheats_vblank()
{
	if (!aw_active())
		return;

	u32 f1 = ReadMem32_nommu(kDbgFlags1);
	u32 want1 = 0;
	if (g_aw_freeplay)
		want1 |= 0x04000000u;   // QUICK FREE
	if (g_aw_ldm_any)
		want1 |= 0x20000000u;   // L CMD FREE
	if (want1)
		WriteMem32_nommu(kDbgFlags1, f1 | want1);

	u32 f2 = ReadMem32_nommu(kDbgFlags2);
	u32 want2 = 0;
	if (g_aw_all_bosses)
		want2 |= 0x0000007Fu;   // PS GAI..MAGAKI
	if (g_aw_same_chars)
		want2 |= 0x00000080u;   // PS SAME
	if (want2)
		WriteMem32_nommu(kDbgFlags2, f2 | want2);

	if (g_aw_all_bosses)
		WriteMem8_nommu(kUnlockMask, 0xFFu);
}
