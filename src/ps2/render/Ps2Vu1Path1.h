#pragma once

#ifdef PS2_PLATFORM

#include "ps2/render/Ps2Vu1DmaQueue.h"

// Terrain VU1 packets use GIF Path1 while gsKit uses Path3. These helpers
// serialize ownership transitions and invalidate GS state before Path3 resumes.
bool ps2_render_acquire_path1();
void ps2_render_barrier_path1();
void ps2_render_release_path1();

bool ps2_vu1_path1_disabled();
void ps2_vu1_path1_disable(const char* reason);
bool ps2_vu1_path1_ensure_terrain_program();
bool ps2_vu1_path1_send_normal_and_wait(const void* data, int qwords,
                                         const char* site);
bool ps2_vu1_path1_send_flusha();

#endif // PS2_PLATFORM
