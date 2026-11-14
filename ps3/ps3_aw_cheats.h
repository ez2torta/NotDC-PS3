#pragma once

// Built-in KOF XI AW toggles. They write the game's own debug/lock flags each vblank.
extern int g_aw_all_bosses;
extern int g_aw_same_chars;
extern int g_aw_ldm_any;

// Latch the console-safe defaults: 0 off, 1 infinite/full, 2 one hit (health only).
extern int g_aw_health[2];
extern int g_aw_power[2];    // power gauge (super)
extern int g_aw_skill[2];    // skill gauge (tag)
extern int g_aw_round_time;  // 0 off, else the seconds left on the round clock (1..98)
extern int g_aw_time_stop;

// Profile row: 0 ARCADE DELUXE (all three on), 1 VANILLA (all off), 2 CUSTOM (a mix). The toggles are the truth.
extern int g_aw_profile;
int  aw_profile_current();
void aw_profile_toggle();   // DELUXE <-> VANILLA; a CUSTOM mix goes to DELUXE

void aw_cheats_vblank();
