// Atomiswave in-game mini menu. Strings live here (-mminimal-toc).
#include "ps3/ps3_aw_mini.h"
#include "ps3/ps3_menu_ui.h"

static const char* const k_row[6] = {
	"Continue", "Buttons", "Test menu", "Cheats", "Advanced options", "Close game",
};
static const char* const k_help[6] = {
	"Back to the game.",
	"Remap A B C D E START COIN for player 1 and 2.",
	"Opens the game's test menu: coin settings, free play, difficulty. Leave it with SAVE&EXIT.",
	"Free Play, All Bosses, Same Chars, LDM Any.",
	"The emulator's own pages: video, audio, speed.",
	"Stop the game and go back to the game list.",
};

static int s_row;

void aw_mini_open() { s_row = 0; }

int aw_mini_input(u32 pressed)
{
	if (pressed & MB_UP)
		s_row = s_row > 0 ? s_row - 1 : 5;
	else if (pressed & MB_DOWN)
		s_row = s_row < 5 ? s_row + 1 : 0;
	else if (pressed & MB_CROSS)
		return s_row;
	else if (pressed & MB_START)
		return AWM_CONTINUE;
	return AWM_NONE;
}

void aw_mini_draw()
{
	ps3m_header("PAUSED", "ATOMISWAVE");
	for (int i = 0; i < 6; i++)
		ps3m_row(i, i == s_row, false, k_row[i], NULL);
	ps3m_footer(k_help[s_row], "X Select   UP/DOWN Row   SELECT Back to game");
}
