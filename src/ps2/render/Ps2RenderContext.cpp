#ifdef PS2_PLATFORM

#include "ps2/render/Ps2RenderContext.h"

Ps2RenderContext& ps2_render_context()
{
    static Ps2RenderContext context;
    return context;
}

#endif // PS2_PLATFORM
