#ifdef PS2_PLATFORM

#include "ps2/render/Ps2RenderFrame.h"

#include "ps2/render/Ps2Draw2D.h"
#include "ps2/render/Ps2GsQueue.h"
#include "ps2/render/Ps2RenderGsState.h"
#include "ps2/render/Ps2RenderStats.h"
#include "ps2/render/Ps2RenderTypes.h"

#include <gsKit.h>
#include <gsPrimitive.h>

extern GSGLOBAL* gsGlobal;

namespace
{
u8 s_clearR = 0;
u8 s_clearG = 0;
u8 s_clearB = 0;

u8 ps2_clear_channel(float value)
{
    if (value <= 0.0f)
        return 0;
    if (value >= 1.0f)
        return 255;
    return static_cast<u8>(value * 255.0f);
}
} // namespace

void ps2_render_frame_set_clear_color(float red, float green, float blue, float alpha)
{
    (void)alpha;
    s_clearR = ps2_clear_channel(red);
    s_clearG = ps2_clear_channel(green);
    s_clearB = ps2_clear_channel(blue);
}

void ps2_render_frame_clear(unsigned int mask)
{
    if (!gsGlobal)
        return;

    ps2_draw_2d_flush_pending();

    const bool clearColor = (mask & Ps2RenderClearMask::Color) != 0;
    const bool clearDepth = (mask & Ps2RenderClearMask::Depth) != 0;
    ps2_gs_state_invalidate_depth();

    if (clearColor)
    {
        ps2_gs_queue_guard(0);
        ps2_gs_state_apply_color_mask();
        ps2_gs_state_set_zmsk(clearDepth ? 0 : 1);
        gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(s_clearR, s_clearG, s_clearB, 0x80, 0));
        ps2_gs_state_invalidate_after_clear();
    }
    else if (clearDepth)
    {
        ps2_gs_state_clear_depth_only();
    }

    PS2_DEPTH_STAT(if (clearDepth) ps2_render_stats().depthClears++);
}

#endif // PS2_PLATFORM
