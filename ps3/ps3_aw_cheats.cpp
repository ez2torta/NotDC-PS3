#include "ps3/ps3_aw_cheats.h"

#include "dc/mem/sh4_mem.h"
#include "ps3/ps3_aw.h"
#include <stdio.h>

int g_aw_all_bosses;
int g_aw_same_chars;
int g_aw_ldm_any;
int g_aw_profile;

int aw_profile_current()
{
	const int n = (g_aw_all_bosses ? 1 : 0) + (g_aw_same_chars ? 1 : 0) + (g_aw_ldm_any ? 1 : 0);
	return n == 3 ? 0 : n == 0 ? 1 : 2;
}

void aw_profile_toggle()
{
	const int on = aw_profile_current() != 0;   // from DELUXE go off, from anything else go on
	g_aw_all_bosses = g_aw_same_chars = g_aw_ldm_any = on;
	g_aw_profile = aw_profile_current();
	printf("[awcheat] profile -> %s (bosses %d, same %d, ldm %d)\n", on ? "ARCADE DELUXE" : "VANILLA",
	       g_aw_all_bosses, g_aw_same_chars, g_aw_ldm_any);
}

// Offsets from the KOF XI AW executable; the recomp cheat module uses the same addresses.
static const u32 kDbgFlags1 = 0x8C35AB3Cu;
static const u32 kDbgFlags2 = 0x8C35AB40u;
static const u32 kUnlockMask = 0x8C18F09Au;

void aw_cheats_vblank()
{
	if (!aw_active())
		return;

	// One line when the set of cheats in force changes, so a log shows what the game was given.
	static int last = -1;
	const int now = (g_aw_all_bosses ? 1 : 0) | (g_aw_same_chars ? 2 : 0) | (g_aw_ldm_any ? 4 : 0);
	if (now != last)
	{
		printf("[awcheat] active: bosses %d, same %d, ldm %d (flags1 %08X, flags2 %08X)\n", g_aw_all_bosses,
		       g_aw_same_chars, g_aw_ldm_any, ReadMem32_nommu(kDbgFlags1), ReadMem32_nommu(kDbgFlags2));
		last = now;
	}

	u32 f1 = ReadMem32_nommu(kDbgFlags1);
	u32 want1 = 0;
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
