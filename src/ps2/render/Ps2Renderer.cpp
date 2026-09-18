#include "ps2/render/Ps2Renderer.h"

#ifdef PS2_PLATFORM

bool ps2_renderer_capture_frame(Ps2RendererFrame& out)
{
    out.valid = ps2_native_prepare_frame_context(out.native);
    return out.valid;
}

bool ps2_renderer_prepare_translated_context(Ps2NativeDrawContext& out,
                                              const Ps2RendererFrame& frame,
                                              float tx, float ty, float tz,
                                              bool fullyInside)
{
    if (!frame.valid)
    {
        out.valid = false;
        out.fullyInside = fullyInside;
        return false;
    }

    return ps2_native_prepare_translated_context(out, frame.native,
                                                 tx, ty, tz, fullyInside);
}

bool ps2_renderer_prepare_terrain_context(Ps2NativeDrawContext& out,
                                           const Ps2RendererFrame& frame,
                                           float tx, float ty, float tz,
                                           bool fullyInside)
{
    constexpr float sectionHalf = 8.0f;
    constexpr float sectionScale = 1.000001f;
    const float scaleOffset = sectionHalf * (sectionScale - 1.0f);

    if (!ps2_renderer_prepare_translated_context(out, frame,
                                                 tx + scaleOffset,
                                                 ty + scaleOffset,
                                                 tz + scaleOffset,
                                                 fullyInside))
    {
        return false;
    }

    // Match the original WorldRenderer section transform:
    // T(section) * T(-8) * S(1.000001) * T(8).
    // After folding the two translations together, only the first three MVP
    // columns need scaling; the fourth already contains the adjusted offset.
    for (int column = 0; column < 3; ++column)
    {
        const int base = column * 4;
        out.mvp[base + 0] *= sectionScale;
        out.mvp[base + 1] *= sectionScale;
        out.mvp[base + 2] *= sectionScale;
        out.mvp[base + 3] *= sectionScale;
    }

    return true;
}

bool ps2_renderer_draw_prepared(const Ps2NativeMeshView& mesh,
                                const Ps2NativeDrawContext& context)
{
    return ps2_native_draw_mesh_prepared(mesh, context);
}

#endif
