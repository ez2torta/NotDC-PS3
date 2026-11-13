#pragma once

// Built-in KOF XI AW toggles. They write the game's own debug/lock flags each vblank.
extern int g_aw_all_bosses;
extern int g_aw_same_chars;
extern int g_aw_ldm_any;

// Profile row: 0 ARCADE DELUXE (all three on), 1 VANILLA (all off), 2 CUSTOM (a mix). The toggles are the truth.
extern int g_aw_profile;
int  aw_profile_current();
void aw_profile_toggle();   // DELUXE <-> VANILLA; a CUSTOM mix goes to DELUXE

void aw_cheats_vblank();
