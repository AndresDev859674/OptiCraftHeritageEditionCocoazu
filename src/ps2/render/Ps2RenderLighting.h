#pragma once

#ifdef PS2_PLATFORM

#include "ps2/render/Ps2RenderState.h"

#include <cstdint>

struct Ps2LightingArrayState
{
    const void* normals;
    int stride;
    unsigned int type;
    bool enabled;
};

void ps2_lighting_set_enabled(bool enabled);
bool ps2_lighting_enabled();
void ps2_lighting_set_light_enabled(int lightIndex, bool enabled);

Ps2LightingArrayState ps2_lighting_array_state();
void ps2_lighting_set_array(const void* normals, int stride,
                            unsigned int type, bool enabled);
void ps2_lighting_restore_array(const Ps2LightingArrayState& state);
bool ps2_lighting_has_active_normal_array();

void ps2_lighting_lightfv(int lightIndex, unsigned int parameter, const float* params);
void ps2_lighting_light_model_ambient(const float* values);
void ps2_lighting_set_lightmap_enabled(bool enabled);
void ps2_lighting_set_legacy_world_pass(bool enabled);
void ps2_lighting_set_lightmap_coord(float u, float v);
void ps2_lighting_set_lightmap_colors(const std::uint32_t* colors, int count);
const std::uint32_t* ps2_lighting_get_lightmap_colors();
std::uint32_t ps2_lighting_apply_packed_brightness(std::uint32_t color, int brightness);

void ps2_lighting_apply_vertex(int index,
                               unsigned char& red,
                               unsigned char& green,
                               unsigned char& blue);
void ps2_lighting_apply_lightmap(unsigned char& red,
                                 unsigned char& green,
                                 unsigned char& blue);
void ps2_lighting_prepare_render_state(Ps2RenderState& state);

#endif // PS2_PLATFORM
