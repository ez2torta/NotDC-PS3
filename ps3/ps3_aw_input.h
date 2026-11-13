#pragma once
#include "types.h"

// Atomiswave controls per player. g_aw_map[player][control] is the DS3 button (PADBTN_*, ps3_padmap.h)
// that drives the machine control; -1 = none. Coin is shared: either player's coin adds one credit.
enum
{
	AWB_A, AWB_B, AWB_C, AWB_D, AWB_E, AWB_START, AWB_COIN,
	AWB_UP, AWB_DOWN, AWB_LEFT, AWB_RIGHT, AWB_SERVICE, AWB_TEST,
	AWB_COUNT
};

extern int g_aw_map[2][AWB_COUNT];

void aw_input_defaults(int player);
void aw_input_load();
void aw_input_save();
// Gives DS3 button b to control c of the player; the control that had b takes c's old button.
void aw_input_set(int player, int c, int b);
// Active-low DC pad word of the player from the DS3 travel of each button (PADBTN_COUNT entries).
u16  aw_input_kcode(int player, const unsigned char* ana);
// Holds TEST on player 1 for the next polls (the Test menu entries and the first seconds of a boot).
void aw_input_test_hold(int polls);
// Bit c set: control c is held now (for the live readout).
u32  aw_input_held(int player, u32 ds3_bits);
