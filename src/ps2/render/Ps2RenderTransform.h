#pragma once

#ifdef PS2_PLATFORM

#include "ps2/render/Ps2ClipGuard.h"

void ps2_transform_frustum(float left, float right, float bottom, float top,
                           float nearValue, float farValue);
void ps2_transform_ortho(float left, float right, float bottom, float top,
                         float nearValue, float farValue);
void ps2_transform_matrix_mode(unsigned int mode);
void ps2_transform_load_identity();
void ps2_transform_push_matrix();
void ps2_transform_pop_matrix();
void ps2_transform_get_matrix(unsigned int query, float* values);
void ps2_transform_translate(float x, float y, float z);
void ps2_transform_scale(float x, float y, float z);
void ps2_transform_rotate(float angle, float axisX, float axisY, float axisZ);
void ps2_transform_viewport(int x, int y, int width, int height);
void ps2_transform_get_viewport(int* values);

void ps2_transform_set_force_near_z(bool enabled);
bool ps2_transform_force_near_z_enabled();
const Ps2DepthMap& ps2_transform_depth_map();

#endif // PS2_PLATFORM
