#include "ps2/render/Ps2Vu0Math.h"

#ifdef PS2_PLATFORM

namespace
{
// Distance of the neutral padding plane. It has to survive both comparisons the
// classifier makes without an infinity, because VU0 clamps an overflow to the
// maximum magnitude rather than producing one. Any value far above the largest
// real center distance works; this is the same magnitude Ps2MeshSort seeds its
// empty cluster bounds with.
constexpr float kNeutralPlaneDistance = 1e30f;

inline float absf(float value)
{
    return value < 0.0f ? -value : value;
}
}

void ps2_vu0_plane_block_build(Ps2Vu0PlaneBlock& out,
                               const float planes[][4],
                               int planeCount)
{
    if (planeCount < 0)
        planeCount = 0;
    if (planeCount > 4)
        planeCount = 4;

    for (int lane = 0; lane < 4; ++lane)
    {
        // A zero normal makes the projected radius zero regardless of the
        // cluster extent, so the constant term alone decides the padding lane.
        const bool padding = lane >= planeCount || planes == nullptr;
        const float x = padding ? 0.0f : planes[lane][0];
        const float y = padding ? 0.0f : planes[lane][1];
        const float z = padding ? 0.0f : planes[lane][2];
        const float w = padding ? kNeutralPlaneDistance : planes[lane][3];

        out.coeff[0 + lane] = x;
        out.coeff[4 + lane] = y;
        out.coeff[8 + lane] = z;
        out.coeff[12 + lane] = w;

        out.absCoeff[0 + lane] = absf(x);
        out.absCoeff[4 + lane] = absf(y);
        out.absCoeff[8 + lane] = absf(z);
    }
}

#endif // PS2_PLATFORM
