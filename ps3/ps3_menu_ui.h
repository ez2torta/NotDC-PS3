#pragma once
#include "types.h"

// Menu pad bits (one per pressed button, edge-triggered by ps3_menu.cpp) and the few drawing
// helpers the Atomiswave screens in their own files (-mminimal-toc) borrow from the menu.
enum {
	MB_UP = 1, MB_DOWN = 2, MB_LEFT = 4, MB_RIGHT = 8,
	MB_CROSS = 16, MB_CIRCLE = 32, MB_TRIANGLE = 64, MB_SQUARE = 128,
	MB_L1 = 256, MB_R1 = 512, MB_START = 1024, MB_SELECT = 2048,
};

void ps3m_header(const char* title, const char* right);
void ps3m_row(int slot, bool selected, bool dim, const char* left, const char* right);
// Three columns (label, player 1, player 2) for the Atomiswave two-pad BUTTONS screen.
void ps3m_row2(int slot, bool selected, bool dim, const char* left, const char* c1, const char* c2);
void ps3m_footer(const char* help, const char* keys);
// PADBTN_* bits (ps3_padmap.h) held on the pad of player 0 or 1, in the order the pads are connected.
// -1: that player has no pad.
int  ps3m_pad_raw(int player);
