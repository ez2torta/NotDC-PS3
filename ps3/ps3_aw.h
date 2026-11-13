#pragma once
#include "types.h"

// Atomiswave boot entry. aw_prepare() only parses the .aw descriptor; aw_boot() loads fmem1,
// opens the virtual cart and leaves the SH-4 at the game's entry point after mem_Reset().

extern bool g_aw_active;
extern bool g_aw_pending;

inline bool aw_active() { return g_aw_active; }
inline bool aw_pending() { return g_aw_pending; }

bool aw_descriptor_path(const char* path);
bool aw_prepare(const char* descriptor);
bool aw_boot();
void aw_shutdown();
void aw_save_flash();

void aw_dma_start(u32 data);
void aw_dma_enable(u32 data);
void aw_trace(const char* fmt, ...);
