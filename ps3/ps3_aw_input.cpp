// Atomiswave: per-player machine control -> DS3 button map, kept in the [ps3] section of the cfg.
#include <stdio.h>
#include "types.h"
#include "config/config.h"
#include "ps3/ps3_aw_input.h"
#include "ps3/ps3_padmap.h"
#include "ps3/dc_pad_bits.h"


int g_aw_map[2][AWB_COUNT];

// Machine control -> DC bit. The game's B/C/D labels sit on DC X/B/Y and E on DC C (docs/specs/botones.md).
static const unsigned short k_dcbit[AWB_COUNT] = {
	key_CONT_A, key_CONT_X, key_CONT_B, key_CONT_Y, key_CONT_C, key_CONT_START, key_CONT_D,
	key_CONT_DPAD_UP, key_CONT_DPAD_DOWN, key_CONT_DPAD_LEFT, key_CONT_DPAD_RIGHT,
	key_CONT_DPAD2_UP, key_CONT_DPAD2_DOWN,
};

static const char* const k_key[AWB_COUNT] = {
	"A", "B", "C", "D", "E", "Start", "Coin", "Up", "Down", "Left", "Right", "Service", "Test",
};

void aw_input_defaults(int p)
{
	static const int def[AWB_COUNT] = {
		PADBTN_SQUARE, PADBTN_CROSS, PADBTN_TRIANGLE, PADBTN_CIRCLE, PADBTN_R1, PADBTN_START, PADBTN_L1,
		PADBTN_UP, PADBTN_DOWN, PADBTN_LEFT, PADBTN_RIGHT, PADBTN_L3, PADBTN_R3,
	};
	for (int c = 0; c < AWB_COUNT; c++)
		g_aw_map[p][c] = def[c];
	// Test and service belong to the first player's pad only.
	if (p)
		g_aw_map[p][AWB_SERVICE] = g_aw_map[p][AWB_TEST] = -1;
}

void aw_input_load()
{
	char key[24];
	for (int p = 0; p < 2; p++)
	{
		aw_input_defaults(p);
		for (int c = 0; c < AWB_COUNT; c++)
		{
			snprintf(key, sizeof(key), "AwP%d%s", p + 1, k_key[c]);
			const int v = cfgLoadInt("ps3", key, g_aw_map[p][c]);
			g_aw_map[p][c] = (v >= -1 && v < PADBTN_COUNT) ? v : g_aw_map[p][c];
		}
	}
}

void aw_input_save()
{
	char key[24];
	for (int p = 0; p < 2; p++)
		for (int c = 0; c < AWB_COUNT; c++)
		{
			snprintf(key, sizeof(key), "AwP%d%s", p + 1, k_key[c]);
			cfgSaveInt("ps3", key, g_aw_map[p][c]);
		}
}

void aw_input_set(int p, int c, int b)
{
	if (p < 0 || p > 1 || c < 0 || c >= AWB_COUNT || b < 0 || b >= PADBTN_COUNT)
		return;
	// The directions may share a button with nothing else, so only the face/shoulder controls swap.
	for (int o = 0; o < AWB_COUNT; o++)
		if (o != c && g_aw_map[p][o] == b)
			g_aw_map[p][o] = g_aw_map[p][c];
	g_aw_map[p][c] = b;
}

u16 aw_input_kcode(int p, const unsigned char* ana)
{
	u16 k = 0xFFFF;
	if (p < 0 || p > 1)
		return k;
	for (int c = 0; c < AWB_COUNT; c++)
	{
		const int b = g_aw_map[p][c];
		if (b >= 0 && ana[b])
			k &= (u16)~k_dcbit[c];
	}
	return k;
}

u32 aw_input_held(int p, u32 ds3)
{
	u32 held = 0;
	if (p < 0 || p > 1)
		return 0;
	for (int c = 0; c < AWB_COUNT; c++)
	{
		const int b = g_aw_map[p][c];
		if (b >= 0 && ((ds3 >> b) & 1))
			held |= 1u << c;
	}
	return held;
}
