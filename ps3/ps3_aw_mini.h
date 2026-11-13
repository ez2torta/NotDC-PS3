#pragma once
#include "types.h"

// In-game menu of an Atomiswave run: six rows instead of the emulator's 17 pages.
enum { AWM_NONE = -1, AWM_CONTINUE, AWM_BUTTONS, AWM_TEST, AWM_CHEATS, AWM_ADVANCED, AWM_CLOSE };

void aw_mini_open();
int  aw_mini_input(u32 pressed);   // the row picked with X, else AWM_NONE
void aw_mini_draw();
const char* aw_credit_line();   // who NotDC AW is based on, shown on the AW home
