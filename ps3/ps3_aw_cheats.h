#pragma once

// Built-in KOF XI AW toggles. They write the game's own debug/lock flags each vblank.
extern int g_aw_all_bosses;
extern int g_aw_same_chars;
extern int g_aw_ldm_any;

void aw_cheats_vblank();
