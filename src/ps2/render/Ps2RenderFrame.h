#pragma once

#ifdef PS2_PLATFORM

void ps2_render_frame_set_clear_color(float red, float green, float blue, float alpha);
void ps2_render_frame_clear(unsigned int mask);

#endif // PS2_PLATFORM
