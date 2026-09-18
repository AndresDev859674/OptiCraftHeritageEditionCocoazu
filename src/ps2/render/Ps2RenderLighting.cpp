#ifdef PS2_PLATFORM

#include "ps2/render/Ps2RenderLighting.h"

#include "ps2/render/Ps2MatrixStack.h"
#include "ps2/render/Ps2RenderTypes.h"
#include "net/minecraft/src/legacy/LegacyLook.h"

#include <cmath>
#include <cstring>

namespace
{
constexpr unsigned int kTypeByte = 0x1400;
constexpr unsigned int kTypeFloat = 0x1406;
constexpr unsigned int kLightDiffuse = ps2RenderValue(Ps2RenderLightParameter::Diffuse);
constexpr unsigned int kLightPosition = ps2RenderValue(Ps2RenderLightParameter::Position);

bool s_lightingOn = false;
bool s_lightOn[2] = { false, false };
float s_lightDirEye[2][3] = { {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f} };
float s_lightDiffuse[2][3] = { {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f} };
float s_lightModelAmbient[3] = { 0.2f, 0.2f, 0.2f };
const void* s_np = nullptr;
int s_nstride = 0;
unsigned int s_ntype = kTypeByte;
bool s_nen = false;

float s_normalMatrix[9] = {
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f
};

bool s_lightmapEnabled = false;
bool s_legacyWorldPass = false;
bool s_lightmapColorsValid = false;
float s_lightmapU = 0.0f;
float s_lightmapV = 0.0f;
std::uint32_t s_lightmapColors[256];

inline void ps2_lighting_grade_material_color(unsigned char& red,
                                              unsigned char& green,
                                              unsigned char& blue)
{
    if (!s_legacyWorldPass)
        return;

    red = legacyLookByte(red);
    green = legacyLookByte(green);
    blue = legacyLookByte(blue);
}

// Memo of the diffuse factor produced by one source normal.
//
// The factor costs a normal-matrix transform, a square root and a divide, and
// ps2_lighting_apply_vertex is called once per vertex through a function
// pointer. Model geometry asks for far fewer distinct normals than that:
// TexturedQuad::emitInto emits four vertices sharing one face normal, and a
// ModelBox has six faces, so the 24 vertices of a cube carry six distinct
// normals. Eight slots cover a cube outright.
//
// The key is the source normal compared exactly, so a hit returns the value the
// long path would have produced bit for bit. Everything the factor depends on
// besides the normal -- the normal matrix, both light directions and colors, the
// model ambient and the per-light enables -- resets the memo when it changes.
constexpr int kLightFactorSlots = 8;

struct LightFactor
{
    float nx, ny, nz;
    float r, g, b;
    bool degenerate; // zero-length eye normal: leave the vertex color untouched
};

LightFactor s_lightFactors[kLightFactorSlots];
int s_lightFactorCount = 0;
int s_lightFactorVictim = 0;
int s_lightFactorLast = 0;

void ps2_light_factor_reset()
{
    s_lightFactorCount = 0;
    s_lightFactorVictim = 0;
    s_lightFactorLast = 0;
}

// Probe the slot that answered last before scanning: consecutive vertices of one
// quad share a normal, so three of every four lookups end here.
const LightFactor* ps2_light_factor_find(float nx, float ny, float nz)
{
    if (s_lightFactorCount <= 0)
        return nullptr;

    const LightFactor& recent = s_lightFactors[s_lightFactorLast];
    if (recent.nx == nx && recent.ny == ny && recent.nz == nz)
        return &recent;

    for (int slot = 0; slot < s_lightFactorCount; ++slot)
    {
        const LightFactor& entry = s_lightFactors[slot];
        if (entry.nx == nx && entry.ny == ny && entry.nz == nz)
        {
            s_lightFactorLast = slot;
            return &entry;
        }
    }
    return nullptr;
}

// Claim a slot for a normal the memo does not hold yet. Replacement is
// round-robin: a model part never needs more slots than it has faces, and a
// stream that does exceed them degrades to the uncached cost rather than
// preferring any one normal.
LightFactor& ps2_light_factor_insert(float nx, float ny, float nz)
{
    int slot;
    if (s_lightFactorCount < kLightFactorSlots)
    {
        slot = s_lightFactorCount++;
    }
    else
    {
        slot = s_lightFactorVictim;
        s_lightFactorVictim = slot + 1 < kLightFactorSlots ? slot + 1 : 0;
    }

    LightFactor& entry = s_lightFactors[slot];
    entry.nx = nx;
    entry.ny = ny;
    entry.nz = nz;
    s_lightFactorLast = slot;
    return entry;
}

void ps2_prepare_normal_matrix()
{
    // Reset before the early return below, not after the matrix is written.
    ps2_light_factor_reset();

    const float* matrix = ps2_matrix_model_view();
    const float a = matrix[0], b = matrix[4], c = matrix[8];
    const float d = matrix[1], e = matrix[5], f = matrix[9];
    const float g = matrix[2], h = matrix[6], i = matrix[10];

    const float c00 = e * i - f * h;
    const float c01 = f * g - d * i;
    const float c02 = d * h - e * g;
    const float c10 = c * h - b * i;
    const float c11 = a * i - c * g;
    const float c12 = b * g - a * h;
    const float c20 = b * f - c * e;
    const float c21 = c * d - a * f;
    const float c22 = a * e - b * d;
    const float determinant = a * c00 + b * c01 + c * c02;

    if (std::fabs(determinant) <= 1.0e-12f)
    {
        s_normalMatrix[0] = a; s_normalMatrix[1] = b; s_normalMatrix[2] = c;
        s_normalMatrix[3] = d; s_normalMatrix[4] = e; s_normalMatrix[5] = f;
        s_normalMatrix[6] = g; s_normalMatrix[7] = h; s_normalMatrix[8] = i;
        return;
    }

    const float inverseDeterminant = 1.0f / determinant;
    s_normalMatrix[0] = c00 * inverseDeterminant;
    s_normalMatrix[1] = c01 * inverseDeterminant;
    s_normalMatrix[2] = c02 * inverseDeterminant;
    s_normalMatrix[3] = c10 * inverseDeterminant;
    s_normalMatrix[4] = c11 * inverseDeterminant;
    s_normalMatrix[5] = c12 * inverseDeterminant;
    s_normalMatrix[6] = c20 * inverseDeterminant;
    s_normalMatrix[7] = c21 * inverseDeterminant;
    s_normalMatrix[8] = c22 * inverseDeterminant;
}

unsigned char ps2_scale_color(unsigned char color, float factor)
{
    const float value = (float)color * factor;
    if (value <= 0.0f) return 0;
    if (value >= 255.0f) return 255;
    return (unsigned char)value;
}
} // namespace

void ps2_lighting_set_enabled(bool enabled)
{
    s_lightingOn = enabled;
    ps2_light_factor_reset();
}

bool ps2_lighting_enabled()
{
    return s_lightingOn;
}

void ps2_lighting_set_light_enabled(int lightIndex, bool enabled)
{
    if (lightIndex >= 0 && lightIndex < 2)
    {
        s_lightOn[lightIndex] = enabled;
        ps2_light_factor_reset();
    }
}

Ps2LightingArrayState ps2_lighting_array_state()
{
    Ps2LightingArrayState state;
    state.normals = s_np;
    state.stride = s_nstride;
    state.type = s_ntype;
    state.enabled = s_nen;
    return state;
}

void ps2_lighting_set_array(const void* normals, int stride,
                            unsigned int type, bool enabled)
{
    s_np = normals;
    s_nstride = stride;
    s_ntype = type;
    s_nen = enabled;
}

void ps2_lighting_restore_array(const Ps2LightingArrayState& state)
{
    ps2_lighting_set_array(state.normals, state.stride, state.type, state.enabled);
}

bool ps2_lighting_has_active_normal_array()
{
    return s_lightingOn && s_nen && s_np != nullptr;
}

void ps2_lighting_lightfv(int lightIndex, unsigned int parameter, const float* params)
{
    if (lightIndex < 0 || lightIndex >= 2 || params == nullptr)
        return;

    if (parameter == kLightPosition)
    {
        const float* matrix = ps2_matrix_model_view();
        const float x = params[0], y = params[1], z = params[2];
        float ex = matrix[0] * x + matrix[4] * y + matrix[8] * z;
        float ey = matrix[1] * x + matrix[5] * y + matrix[9] * z;
        float ez = matrix[2] * x + matrix[6] * y + matrix[10] * z;
        const float lengthSquared = ex * ex + ey * ey + ez * ez;
        if (lengthSquared < 1.0e-12f)
            return;

        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        s_lightDirEye[lightIndex][0] = ex * inverseLength;
        s_lightDirEye[lightIndex][1] = ey * inverseLength;
        s_lightDirEye[lightIndex][2] = ez * inverseLength;
        ps2_light_factor_reset();
    }
    else if (parameter == kLightDiffuse)
    {
        s_lightDiffuse[lightIndex][0] = params[0];
        s_lightDiffuse[lightIndex][1] = params[1];
        s_lightDiffuse[lightIndex][2] = params[2];
        ps2_light_factor_reset();
    }
}

void ps2_lighting_light_model_ambient(const float* values)
{
    if (values == nullptr)
        return;

    s_lightModelAmbient[0] = values[0];
    s_lightModelAmbient[1] = values[1];
    s_lightModelAmbient[2] = values[2];
    ps2_light_factor_reset();
}

void ps2_lighting_set_lightmap_enabled(bool enabled)
{
    s_lightmapEnabled = enabled;
}

void ps2_lighting_set_legacy_world_pass(bool enabled)
{
    s_legacyWorldPass = enabled;
}

void ps2_lighting_set_lightmap_coord(float u, float v)
{
    s_lightmapU = u;
    s_lightmapV = v;
}

void ps2_lighting_set_lightmap_colors(const std::uint32_t* colors, int count)
{
    if (colors == nullptr || count < 256)
    {
        s_lightmapColorsValid = false;
        return;
    }

    std::memcpy(s_lightmapColors, colors, sizeof(s_lightmapColors));
    s_lightmapColorsValid = true;
}

const std::uint32_t* ps2_lighting_get_lightmap_colors()
{
    return s_lightmapColorsValid ? s_lightmapColors : nullptr;
}

std::uint32_t ps2_lighting_apply_packed_brightness(std::uint32_t color, int brightness)
{
    if (!s_lightmapColorsValid)
        return color;

    const int sky = (brightness >> 20) & 15;
    const int block = (brightness >> 4) & 15;
    const std::uint32_t light = s_lightmapColors[sky * 16 + block];
    unsigned char red = static_cast<unsigned char>(color & 0xffu);
    unsigned char green = static_cast<unsigned char>((color >> 8) & 0xffu);
    unsigned char blue = static_cast<unsigned char>((color >> 16) & 0xffu);
    const unsigned int alpha = (color >> 24) & 0xffu;
    ps2_lighting_grade_material_color(red, green, blue);
    const unsigned int lightRed = (light >> 16) & 0xffu;
    const unsigned int lightGreen = (light >> 8) & 0xffu;
    const unsigned int lightBlue = light & 0xffu;
    const unsigned int litRed = (static_cast<unsigned int>(red) * lightRed + 127u) / 255u;
    const unsigned int litGreen = (static_cast<unsigned int>(green) * lightGreen + 127u) / 255u;
    const unsigned int litBlue = (static_cast<unsigned int>(blue) * lightBlue + 127u) / 255u;
    return (alpha << 24) | (litBlue << 16) | (litGreen << 8) | litRed;
}

void ps2_lighting_apply_vertex(int index,
                               unsigned char& red,
                               unsigned char& green,
                               unsigned char& blue)
{
    if (!s_lightingOn || !s_nen || s_np == nullptr)
        return;
    if (!s_lightOn[0] && !s_lightOn[1])
        return;

    float nx, ny, nz;
    if (s_ntype == kTypeFloat)
    {
        const int stride = s_nstride ? s_nstride : (int)(3 * sizeof(float));
        const float* normal = (const float*)((const char*)s_np + index * stride);
        nx = normal[0]; ny = normal[1]; nz = normal[2];
    }
    else
    {
        const int stride = s_nstride ? s_nstride : 3;
        const signed char* normal = (const signed char*)s_np + index * stride;
        nx = (float)normal[0] * (1.0f / 127.0f);
        ny = (float)normal[1] * (1.0f / 127.0f);
        nz = (float)normal[2] * (1.0f / 127.0f);
    }

    const LightFactor* factor = ps2_light_factor_find(nx, ny, nz);
    if (factor == nullptr)
    {
        LightFactor& entry = ps2_light_factor_insert(nx, ny, nz);

        const float ex = s_normalMatrix[0] * nx + s_normalMatrix[1] * ny + s_normalMatrix[2] * nz;
        const float ey = s_normalMatrix[3] * nx + s_normalMatrix[4] * ny + s_normalMatrix[5] * nz;
        const float ez = s_normalMatrix[6] * nx + s_normalMatrix[7] * ny + s_normalMatrix[8] * nz;
        const float lengthSquared = ex * ex + ey * ey + ez * ez;
        entry.degenerate = lengthSquared < 1.0e-12f;
        if (!entry.degenerate)
        {
            const float inverseLength = 1.0f / std::sqrt(lengthSquared);
            float factorR = s_lightModelAmbient[0];
            float factorG = s_lightModelAmbient[1];
            float factorB = s_lightModelAmbient[2];

            for (int light = 0; light < 2; ++light)
            {
                if (!s_lightOn[light])
                    continue;

                const float diffuse = (ex * s_lightDirEye[light][0] +
                                       ey * s_lightDirEye[light][1] +
                                       ez * s_lightDirEye[light][2]) * inverseLength;
                if (diffuse <= 0.0f)
                    continue;

                factorR += diffuse * s_lightDiffuse[light][0];
                factorG += diffuse * s_lightDiffuse[light][1];
                factorB += diffuse * s_lightDiffuse[light][2];
            }

            if (factorR > 1.0f) factorR = 1.0f;
            if (factorG > 1.0f) factorG = 1.0f;
            if (factorB > 1.0f) factorB = 1.0f;

            entry.r = factorR;
            entry.g = factorG;
            entry.b = factorB;
        }
        factor = &entry;
    }

    if (factor->degenerate)
        return;

    red = ps2_scale_color(red, factor->r);
    green = ps2_scale_color(green, factor->g);
    blue = ps2_scale_color(blue, factor->b);
    if (!s_lightmapEnabled)
        ps2_lighting_grade_material_color(red, green, blue);
}

void ps2_lighting_apply_lightmap(unsigned char& red,
                                 unsigned char& green,
                                 unsigned char& blue)
{
    if (!s_lightmapEnabled || !s_lightmapColorsValid)
        return;

    ps2_lighting_grade_material_color(red, green, blue);

    int blockLight = (int)s_lightmapU >> 4;
    int skyLight = (int)s_lightmapV >> 4;
    if (blockLight < 0) blockLight = 0; else if (blockLight > 15) blockLight = 15;
    if (skyLight < 0) skyLight = 0; else if (skyLight > 15) skyLight = 15;

    const std::uint32_t light = s_lightmapColors[skyLight * 16 + blockLight];
    const unsigned int lightR = (light >> 16) & 0xffu;
    const unsigned int lightG = (light >> 8) & 0xffu;
    const unsigned int lightB = light & 0xffu;
    red = (unsigned char)(((unsigned int)red * lightR + 127u) / 255u);
    green = (unsigned char)(((unsigned int)green * lightG + 127u) / 255u);
    blue = (unsigned char)(((unsigned int)blue * lightB + 127u) / 255u);
}

void ps2_lighting_prepare_render_state(Ps2RenderState& state)
{
    if (ps2_lighting_has_active_normal_array())
    {
        ps2_prepare_normal_matrix();
        state.lightVertex = ps2_lighting_apply_vertex;
    }
    else
    {
        state.lightVertex = nullptr;
    }
}

#endif // PS2_PLATFORM
