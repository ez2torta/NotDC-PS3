#pragma once
#include "types.h"

// Atomiswave BUTTONS screen: quick remap of A B C D E START COIN for the two players.
void aw_remap_open();
// pressed: MB_* bits. true when the player leaves the screen (settings saved).
bool aw_remap_input(u32 pressed);
void aw_remap_draw();
