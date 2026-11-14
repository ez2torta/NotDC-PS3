#include "ps3/ps3_aw_cheats.h"

#include "dc/mem/sh4_mem.h"
#include "ps3/ps3_aw.h"
#include "ps3/ps3_aw_input.h"
#include <stdio.h>

int g_aw_all_bosses;
int g_aw_same_chars;
int g_aw_ldm_any;
int g_aw_profile;

int g_aw_health[2];
int g_aw_power[2];
int g_aw_skill[2];
int g_aw_round_time;
int g_aw_time_stop;

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
static const u32 kTeam0 = 0x8C27CB54u;       // team block, +0x1F8 for player 2
static const u32 kTeamStride = 0x1F8u;
static const u32 kTeamPoint = 0x53u;         // active team member, 0..2
static const u32 kTeamHealth = 0x150u;       // + point*0x20 + 4, u16 (112 = full)
static const u32 kTeamSuper = 0x88u;         // power gauge, u32
static const u32 kTeamSkill = 0x8Cu;         // skill/tag gauge, u32
static const u32 kRoundClock = 0x8C27CB78u;  // u16 seconds, 99 at round start
static const u32 kDbgFlags0 = 0x8C35AB38u;   // group 0: Time Stop = 0x10
static const u32 kDbgFlags1 = 0x8C35AB3Cu;   // group 1: L CMD FREE = 0x20000000
static const u32 kDbgFlags2 = 0x8C35AB40u;   // group 2: PS GAI..MAGAKI = 0x7F, PS SAME = 0x80
static const u32 kUnlockMask = 0x8C18F09Au;

static const u32 kDbgLdmAny = 0x20000000u;
static const u32 kDbgTimeStop = 0x10u;
static const u16 kMaxHealth = 112;
static const u32 kGaugeFull = 0xE0u;

// Bits this emulator set in each debug word, so turning a cheat off clears only its own bit.
static u32 s_owned0, s_owned1, s_owned2;
static u8 s_life_prev[2];

static void write_flags(u32 addr, u32 want, u32* owned)
{
	const u32 now = ReadMem32_nommu(addr);
	const u32 next = (now & ~*owned) | want;
	if (next != now)
		WriteMem32_nommu(addr, next);
	*owned = want;
}

// One line when the set of cheats in force changes, so a log shows what the game was given.
static void log_state()
{
	static int last = -1;
	const int now = (g_aw_all_bosses ? 1 : 0) | (g_aw_same_chars ? 2 : 0) | (g_aw_ldm_any ? 4 : 0) |
	                (g_aw_time_stop ? 8 : 0) | (g_aw_round_time ? 16 : 0) |
	                (g_aw_health[0] ? 32 : 0) | (g_aw_health[1] ? 64 : 0) |
	                (g_aw_power[0] ? 128 : 0) | (g_aw_power[1] ? 256 : 0) |
	                (g_aw_skill[0] ? 512 : 0) | (g_aw_skill[1] ? 1024 : 0);
	if (now == last)
		return;
	last = now;
	printf("[awcheat] active: bosses %d same %d ldm %d time %d/%d stop %d health %d/%d power %d/%d skill %d/%d\n",
	       g_aw_all_bosses, g_aw_same_chars, g_aw_ldm_any, g_aw_round_time > 0, g_aw_round_time,
	       g_aw_time_stop, g_aw_health[0], g_aw_health[1], g_aw_power[0], g_aw_power[1],
	       g_aw_skill[0], g_aw_skill[1]);
}

void aw_cheats_vblank()
{
	if (!aw_active())
		return;

	// TEST is held for a frame count, not for pad polls: the game may take seconds to
	// reach the routine that opens its test menu.
	aw_input_test_tick();
	log_state();

	write_flags(kDbgFlags0, g_aw_time_stop ? kDbgTimeStop : 0u, &s_owned0);
	write_flags(kDbgFlags1, g_aw_ldm_any ? kDbgLdmAny : 0u, &s_owned1);

	u32 want2 = 0;
	if (g_aw_all_bosses)
		want2 |= 0x0000007Fu;   // PS GAI..MAGAKI
	if (g_aw_same_chars)
		want2 |= 0x00000080u;   // PS SAME
	write_flags(kDbgFlags2, want2, &s_owned2);

	if (g_aw_all_bosses)
		WriteMem8_nommu(kUnlockMask, 0xFFu);

	if (g_aw_round_time > 0)
	{
		// Signed: timeover is the clock going below zero (0xFFFF) and must not be refilled.
		const s16 clock = (s16)ReadMem16_nommu(kRoundClock);
		if (clock > (s16)g_aw_round_time && clock <= 99)
			WriteMem16_nommu(kRoundClock, (u16)g_aw_round_time);
	}

	for (int side = 0; side < 2; side++)
	{
		const u32 team = kTeam0 + (u32)side * kTeamStride;
		const u32 point = ReadMem8_nommu(team + kTeamPoint);
		const int life = g_aw_health[side];
		if (point < 3)
		{
			const u32 address = team + kTeamHealth + point * 0x20u + 4u;
			if (life == 1)
				WriteMem16_nommu(address, kMaxHealth);
			else if (life == 2)
				WriteMem16_nommu(address, 1u);
			else if (s_life_prev[side])
				WriteMem16_nommu(address, kMaxHealth);   // give back what the cheat was holding
		}
		s_life_prev[side] = (u8)life;

		if (g_aw_power[side])
			WriteMem32_nommu(team + kTeamSuper, kGaugeFull);
		if (g_aw_skill[side])
			WriteMem32_nommu(team + kTeamSkill, kGaugeFull);
	}
}
