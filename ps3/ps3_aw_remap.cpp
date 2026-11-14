// Atomiswave BUTTONS screen, one column per pad. Strings live here (-mminimal-toc) to spare the menu's TOC.
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

// Page 0: A..COIN (7 rows). Page 1: UP..TEST (6) and BACK (1).
enum { R_BACK = AWB_TEST + 1, PAGE_ROWS = 7 };

struct Cursor
{
	int  row;
	bool cap;       // waiting for a DS3 button for cap_ctl
	bool wait;      // ...until every button is released
	int  cap_ctl;
};

static Cursor s_cur[2];
static u32    s_prev[2];
static int    s_page;

void aw_remap_open()
{
	memset(s_cur, 0, sizeof(s_cur));
	s_page = 0;
	// A button already held when the screen opens (the X that opened it) is not a press.
	const int r0 = ps3m_pad_raw(0), r1 = ps3m_pad_raw(1);
	s_prev[0] = r0 < 0 ? 0 : (u32)r0;
	s_prev[1] = r1 < 0 ? 0 : (u32)r1;
	printf("[awmap] open: P1 pad %s, P2 pad %s\n", r0 < 0 ? "none" : "ok", r1 < 0 ? "none" : "ok");
}

static int row_ctl(int row)
{
	if (s_page == 0)
		return row <= AWB_COIN ? row : -1;
	if (row >= 6)
		return -1;
	return AWB_UP + row;
}

static bool pressed_bit(u32 raw, int bit)
{
	return (raw & (1u << bit)) != 0;
}

static void begin_capture(int p, int ctl)
{
	if (ps3m_pad_raw(p) < 0)
	{
		printf("[awmap] P%d has no pad, remap refused\n", p + 1);
		return;
	}
	s_cur[p].cap_ctl = ctl;
	s_cur[p].cap = true;
	s_cur[p].wait = true;
	printf("[awmap] P%d waiting for a button for %s\n", p + 1, k_short[ctl]);
}

// One player's pad for this frame. Returns the buttons that went down now.
static u32 pad_pressed(int p)
{
	const int raw = ps3m_pad_raw(p);
	if (raw < 0)
	{
		s_prev[p] = 0;
		return 0;
	}
	const u32 now = (u32)raw;
	const u32 pressed = now & ~s_prev[p];
	s_prev[p] = now;
	return pressed;
}

static bool one_player(int p)
{
	Cursor* c = &s_cur[p];
	const int raw = ps3m_pad_raw(p);
	const u32 pr = pad_pressed(p);

	if (c->cap)
	{
		if (raw < 0)
		{
			c->cap = c->wait = false;
			return false;
		}
		if (c->wait)
		{
			c->wait = raw != 0;
			return false;
		}
		if (!raw)
			return false;
		int b = 0;
		while (!((raw >> b) & 1))
			b++;
		aw_input_set(p, c->cap_ctl, b);
		printf("[awmap] P%d %s <- %s\n", p + 1, k_short[c->cap_ctl], k_ds3[b]);
		c->cap = false;
		return false;
	}

	if (pressed_bit(pr, PADBTN_UP))
		c->row = c->row > 0 ? c->row - 1 : PAGE_ROWS - 1;
	else if (pressed_bit(pr, PADBTN_DOWN))
		c->row = c->row < PAGE_ROWS - 1 ? c->row + 1 : 0;
	else if (pressed_bit(pr, PADBTN_TRIANGLE))
	{
		s_page ^= 1;
		if (c->row > PAGE_ROWS - 1)
			c->row = PAGE_ROWS - 1;
	}
	else if (pressed_bit(pr, PADBTN_CIRCLE))
	{
		aw_input_save();
		printf("[awmap] saved\n");
		return true;
	}
	else if (pressed_bit(pr, PADBTN_SQUARE))
	{
		aw_input_defaults(p);
		printf("[awmap] P%d reset to default\n", p + 1);
	}
	else if (pressed_bit(pr, PADBTN_CROSS))
	{
		const int ctl = row_ctl(c->row);
		if (ctl >= 0)
			begin_capture(p, ctl);
		else if (s_page == 1)
		{
			s_page = 0;
			c->row = AWB_COIN;
		}
	}
	return false;
}

bool aw_remap_input(u32 pressed)
{
	// SELECT is the only button that is not in the pad map: it cancels a capture, or saves and exits.
	if (pressed & MB_SELECT)
	{
		if (s_cur[0].cap || s_cur[1].cap)
		{
			s_cur[0].cap = s_cur[0].wait = false;
			s_cur[1].cap = s_cur[1].wait = false;
			printf("[awmap] capture cancelled\n");
			return false;
		}
		aw_input_save();
		printf("[awmap] saved\n");
		return true;
	}

	// Each column reads its own pad, so both players can remap at the same time.
	bool done = false;
	if (one_player(0))
		done = true;
	if (one_player(1))
		done = true;
	return done;
}

void aw_remap_draw()
{
	char v[2][24];
	ps3m_header("BUTTONS", s_page == 0 ? "A-E / START / COIN" : "DIRECTIONS / TEST");

	const int raw0 = ps3m_pad_raw(0), raw1 = ps3m_pad_raw(1);
	ps3m_row2(0, false, true, "", "PLAYER 1", "PLAYER 2");
	ps3m_row(1, false, true, "PAD",
	         raw0 < 0 ? "P1: none" : (raw1 < 0 ? "P1: ok   P2: none" : "P1: ok   P2: ok"));

	for (int i = 0; i < PAGE_ROWS; i++)
	{
		const int ctl = row_ctl(i);
		const char* label = s_page == 1 && i >= 6 ? "BACK to A-E / START / COIN" : k_ctl[ctl];
		for (int p = 0; p < 2; p++)
		{
			Cursor* c = &s_cur[p];
			const bool sel = c->row == i;
			if (ctl < 0)
			{
				snprintf(v[p], sizeof(v[p]), "%s", sel ? ">" : " ");
				continue;
			}
			if (c->cap && c->cap_ctl == ctl)
				snprintf(v[p], sizeof(v[p]), "%sPRESS", sel ? ">" : " ");
			else
			{
				const int b = g_aw_map[p][ctl];
				snprintf(v[p], sizeof(v[p]), "%s%s", sel ? ">" : " ", b >= 0 ? k_ds3[b] : "NONE");
			}
		}
		ps3m_row2(2 + i, s_cur[0].row == i || s_cur[1].row == i, false, label, v[0], v[1]);
	}

	ps3m_footer("Each pad: UP/DOWN row, X remap, [] reset. NONE means unbound.",
	            "TRIANGLE Dirs/Test   O Save   SELECT Cancel");
}
