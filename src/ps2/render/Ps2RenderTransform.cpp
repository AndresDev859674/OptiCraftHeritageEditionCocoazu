#ifdef PS2_PLATFORM

#include "ps2/render/Ps2RenderTransform.h"

#include "ps2/render/Ps2MatrixStack.h"
#include "ps2/render/Ps2ProjectionState.h"
#include "ps2/render/Ps2RenderBackend.h"
#include "ps2/render/Ps2Tuning.h"
#include "ps2/render/Ps2Viewport.h"

#include <cstring>

namespace
{
Ps2DepthMap s_depthMap = {
    64.0f,
    (float)PS2_GS_Z_MAX / (64.0f - 0.05f),
    (float)PS2_GS_Z_MAX * (0.05f * PS2_DEPTH_NEAR_BOOST) * 64.0f /
        (64.0f - 0.05f * PS2_DEPTH_NEAR_BOOST),
    -(float)PS2_GS_Z_MAX * (0.05f * PS2_DEPTH_NEAR_BOOST) /
        (64.0f - 0.05f * PS2_DEPTH_NEAR_BOOST)
};

bool s_forceNearZ = false;

void ps2_transform_set_frustum_planes(float zNear, float zFar)
{
    if (!(zFar > zNear))
        return;

    s_depthMap.zFar = zFar;
    s_depthMap.scale = (float)PS2_GS_Z_MAX / (zFar - zNear);

    float depthNear = zNear * PS2_DEPTH_NEAR_BOOST;
    if (!(depthNear < zFar))
        depthNear = zNear;

    const float depthRange = zFar - depthNear;
    s_depthMap.qScale = (float)PS2_GS_Z_MAX * depthNear * zFar / depthRange;
    s_depthMap.qBias = -(float)PS2_GS_Z_MAX * depthNear / depthRange;
}
} // namespace

void ps2_transform_frustum(float left, float right, float bottom, float top,
                           float nearValue, float farValue)
{
    ps2_transform_set_frustum_planes(nearValue, farValue);

    Ps2Mat4 matrix;
    std::memset(matrix, 0, sizeof(matrix));
    const float invWidth = 1.0f / (right - left);
    const float invHeight = 1.0f / (top - bottom);
    const float invDepth = 1.0f / (farValue - nearValue);
    matrix[0] = 2.0f * nearValue * invWidth;
    matrix[5] = 2.0f * nearValue * invHeight;
    matrix[8] = (right + left) * invWidth;
    matrix[9] = (top + bottom) * invHeight;
    matrix[10] = -(farValue + nearValue) * invDepth;
    matrix[11] = -1.0f;
    matrix[14] = -2.0f * farValue * nearValue * invDepth;
    ps2_matrix_multiply_current(matrix);
}

void ps2_transform_ortho(float left, float right, float bottom, float top,
                         float nearValue, float farValue)
{
    ps2_projection_set_ortho(left, right, bottom, top, nearValue, farValue);
}

void ps2_transform_matrix_mode(unsigned int mode)
{
    ps2_matrix_set_mode(mode);
}

void ps2_transform_load_identity()
{
    if (ps2_matrix_load_identity())
        ps2_projection_clear_ortho();
}

void ps2_transform_push_matrix()
{
    ps2_projection_push(ps2_matrix_current_mode());
    ps2_matrix_push();
}

void ps2_transform_pop_matrix()
{
    ps2_projection_pop(ps2_matrix_current_mode());
    ps2_matrix_pop();
}

void ps2_transform_get_matrix(unsigned int query, float* values)
{
    ps2_matrix_get(query, values);
}

void ps2_transform_translate(float x, float y, float z)
{
    ps2_matrix_translate(x, y, z);
}

void ps2_transform_scale(float x, float y, float z)
{
    ps2_matrix_scale(x, y, z);
}

void ps2_transform_rotate(float angle, float axisX, float axisY, float axisZ)
{
    ps2_matrix_rotate(angle, axisX, axisY, axisZ);
}

void ps2_transform_viewport(int x, int y, int width, int height)
{
    ps2_viewport_set(x, y, width, height);
}

void ps2_transform_get_viewport(int* values)
{
    ps2_viewport_get(values);
}

void ps2_transform_set_force_near_z(bool enabled)
{
    s_forceNearZ = enabled;
}

bool ps2_transform_force_near_z_enabled()
{
    return s_forceNearZ;
}

const Ps2DepthMap& ps2_transform_depth_map()
{
    return s_depthMap;
}

#endif // PS2_PLATFORM
