// Atomiswave BUTTONS screen. Strings live here (-mminimal-toc) to spare the menu's TOC.
#include <stdio.h>
#include <string.h>
#include "ps3/ps3_aw_remap.h"
#include "ps3/ps3_aw_input.h"
#include "ps3/ps3_menu_ui.h"
#include "ps3/ps3_padmap.h"

static const char* const k_ds3[PADBTN_COUNT] = {
	"CROSS", "CIRCLE", "SQUARE", "TRIANGLE", "START", "D-PAD UP", "D-PAD DOWN", "D-PAD LEFT", "D-PAD RIGHT",
	"L1", "R1", "L2", "R2", "L3", "R3",
};

static const char* const k_ctl[AWB_COUNT] = {
	"A   LP  light punch", "B   LK  light kick", "C   HP  heavy punch", "D   HK  heavy kick",
	"E   BLOWBACK", "START", "COIN  (shared credit)",
	"UP", "DOWN", "LEFT", "RIGHT", "SERVICE", "TEST",
};

static const char* const k_short[AWB_COUNT] = {
	"A", "B", "C", "D", "E", "START", "COIN", "UP", "DOWN", "LEFT", "RIGHT", "SERVICE", "TEST",
};

// Page 0: the seven main controls, REMAP ALL, MORE, RESET. Page 1: the other controls and BACK.
enum { R_ALL = AWB_COIN + 1, R_MORE, R_RESET, P0_ROWS };
#define P1_ROWS 7   // UP DOWN LEFT RIGHT SERVICE TEST, BACK

static int  s_player;
static int  s_page;
static int  s_row;
static bool s_capture;      // waiting for a DS3 button for control s_cap_ctl
static bool s_cap_wait;     // ...until every button is released
static bool s_all;          // REMAP ALL walks A..COIN
static int  s_cap_ctl;

void aw_remap_open()
{
	s_player = s_page = s_row = 0;
	s_capture = s_all = false;
	printf("[awmap] open: P1 pad %s, P2 pad %s\n", ps3m_pad_raw(0) < 0 ? "none" : "ok",
	       ps3m_pad_raw(1) < 0 ? "none" : "ok");
}

static int row_ctl(int row)   // control of a page row, -1 for the action rows
{
	if (s_page == 0)
		return row <= AWB_COIN ? row : -1;
	if (row >= 6)
		return -1;
	return AWB_UP + row;
}

static int row_count() { return s_page == 0 ? (int)P0_ROWS : P1_ROWS; }

static void begin_capture(int ctl)
{
	if (ps3m_pad_raw(s_player) < 0)
	{
		printf("[awmap] P%d has no pad, remap refused\n", s_player + 1);
		return;
	}
	s_cap_ctl = ctl;
	s_capture = true;
	s_cap_wait = true;
	printf("[awmap] P%d waiting for a button for %s\n", s_player + 1, k_short[ctl]);
}

bool aw_remap_input(u32 pressed)
{
	if (s_capture)
	{
		if (pressed & MB_SELECT)
		{
			s_capture = s_all = false;
			return false;
		}
		const int raw = ps3m_pad_raw(s_player);
		if (raw < 0)
		{
			s_capture = s_all = false;
			return false;
		}
		if (s_cap_wait)
		{
			s_cap_wait = raw != 0;
			return false;
		}
		if (!raw)
			return false;
		int b = 0;
		while (!((raw >> b) & 1))
			b++;
		aw_input_set(s_player, s_cap_ctl, b);
		printf("[awmap] P%d %s <- %s\n", s_player + 1, k_short[s_cap_ctl], k_ds3[b]);
		if (s_all && s_cap_ctl < AWB_COIN)
		{
			s_cap_ctl++;
			s_row = s_cap_ctl;
			s_cap_wait = true;
		}
		else
			s_capture = s_all = false;
		return false;
	}

	const int n = row_count();
	if (pressed & MB_UP)
		s_row = s_row > 0 ? s_row - 1 : n - 1;
	else if (pressed & MB_DOWN)
		s_row = s_row < n - 1 ? s_row + 1 : 0;
	else if (pressed & (MB_LEFT | MB_RIGHT | MB_L1 | MB_R1))
		s_player ^= 1;
	else if (pressed & MB_CROSS)
	{
		const int c = row_ctl(s_row);
		if (c >= 0)
			begin_capture(c);
		else if (s_page == 0 && s_row == R_ALL)
		{
			s_all = true;
			s_row = 0;
			begin_capture(0);
			s_all = s_capture;
		}
		else if (s_page == 0 && s_row == R_MORE)
		{
			s_page = 1;
			s_row = 0;
		}
		else if (s_page == 0 && s_row == R_RESET)
			aw_input_defaults(s_player), printf("[awmap] P%d reset to default\n", s_player + 1);
		else if (s_page == 1)
		{
			s_page = 0;
			s_row = R_MORE;
		}
	}
	else if (pressed & MB_CIRCLE)
	{
		if (s_page == 1)
		{
			s_page = 0;
			s_row = R_MORE;
		}
		else
		{
			aw_input_save();
			printf("[awmap] saved\n");
			return true;
		}
	}
	return false;
}

void aw_remap_draw()
{
	char l[48], r[48], buf[160];
	const int raw = ps3m_pad_raw(s_player);

	snprintf(buf, sizeof(buf), "PLAYER %d", s_player + 1);
	ps3m_header("BUTTONS", buf);

	int slot = 0;
	snprintf(buf, sizeof(buf), raw < 0 ? "Player %d: no pad connected" : "Player %d: pad connected",
	         s_player + 1);
	ps3m_row(slot++, false, true, buf, NULL);

	const int n = row_count();
	for (int i = 0; i < n; i++)
	{
		const int c = row_ctl(i);
		const bool sel = i == s_row;
		if (c >= 0)
		{
			const int b = g_aw_map[s_player][c];
			if (s_capture && s_cap_ctl == c)
				snprintf(r, sizeof(r), "PRESS A BUTTON");
			else
				snprintf(r, sizeof(r), "%s", b >= 0 ? k_ds3[b] : "NONE");
			ps3m_row(slot++, sel, false, k_ctl[c], r);
			continue;
		}
		if (s_page == 1)
			snprintf(l, sizeof(l), "BACK");
		else if (i == R_ALL)
			snprintf(l, sizeof(l), "REMAP ALL  (A to COIN, in order)");
		else if (i == R_MORE)
			snprintf(l, sizeof(l), "Directions, service, test...");
		else
			snprintf(l, sizeof(l), "RESET THIS PLAYER TO DEFAULT");
		ps3m_row(slot++, sel, false, l, NULL);
	}

	// What the pad drives right now, to check a mapping without leaving the screen.
	buf[0] = '\0';
	const u32 held = aw_input_held(s_player, raw > 0 ? (u32)raw : 0);
	for (int c = 0; c < AWB_COUNT; c++)
		if ((held >> c) & 1)
		{
			if (buf[0])
				strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
			strncat(buf, k_short[c], sizeof(buf) - strlen(buf) - 1);
		}
	snprintf(l, sizeof(l), "Pressing: %s", buf[0] ? buf : "-");
	ps3m_row(slot + 1, false, true, l, NULL);

	if (s_capture)
		ps3m_footer("Press the DS3 button you want. If it was used by another control, they swap.",
		            "SELECT Cancel");
	else
		ps3m_footer("Pick a row and press X, then the button. Both players share the credit.",
		            "X Remap   UP/DOWN Row   LEFT/RIGHT Player   O Back");
}
