#pragma once

#ifdef PS2_PLATFORM

#include <gsKit.h>

void ps2_texture_state_apply_clamp_sel(int mode, int ufix, int vfix);
void ps2_texture_state_set_clamp_for_uv(GSTEXTURE* texture,
                                        float u0, float v0,
                                        float u1, float v1,
                                        float u2, float v2);
void ps2_texture_state_invalidate();

#endif // PS2_PLATFORM
