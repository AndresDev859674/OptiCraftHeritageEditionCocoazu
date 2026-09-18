#ifdef PS2_PLATFORM

#include "ps2/render/Ps2RenderTextureState.h"

#include "ps2/render/Ps2ClipGuard.h"
#include "ps2/render/Ps2ProjectionState.h"
#include "ps2/render/Ps2Texture.h"

extern GSGLOBAL* gsGlobal;

namespace
{
int s_clampMode = -1;
int s_regionUFix = -1;
int s_regionVFix = -1;
} // namespace

void ps2_texture_state_apply_clamp_sel(int mode, int ufix, int vfix)
{
    if (!gsGlobal || !gsGlobal->Clamp)
        return;

    switch (mode)
    {
    case PS2_CLAMPSEL_REPEAT:
        if (s_clampMode != GS_CMODE_REPEAT)
        {
            gsKit_set_clamp(gsGlobal, GS_CMODE_REPEAT);
            s_clampMode = GS_CMODE_REPEAT;
            s_regionUFix = -1;
            s_regionVFix = -1;
        }
        return;

    case PS2_CLAMPSEL_REGION:
        if (s_clampMode == GS_CMODE_REGION_REPEAT &&
            s_regionUFix == ufix && s_regionVFix == vfix)
            return;

        gsGlobal->Clamp->MINU = 15;
        gsGlobal->Clamp->MAXU = ufix;
        gsGlobal->Clamp->MINV = 15;
        gsGlobal->Clamp->MAXV = vfix;
        gsKit_set_clamp(gsGlobal, GS_CMODE_REGION_REPEAT);
        s_clampMode = GS_CMODE_REGION_REPEAT;
        s_regionUFix = ufix;
        s_regionVFix = vfix;
        return;

    default:
        if (s_clampMode != GS_CMODE_CLAMP)
        {
            gsKit_set_clamp(gsGlobal, GS_CMODE_CLAMP);
            s_clampMode = GS_CMODE_CLAMP;
            s_regionUFix = -1;
            s_regionVFix = -1;
        }
        return;
    }
}

void ps2_texture_state_set_clamp_for_uv(GSTEXTURE* texture,
                                        float u0, float v0,
                                        float u1, float v1,
                                        float u2, float v2)
{
    if (!gsGlobal || !gsGlobal->Clamp || !texture)
        return;

    float minU, minV, maxU, maxV;
    ps2_uv_bounds3(u0, v0, u1, v1, u2, v2, minU, minV, maxU, maxV);
    const Ps2ClampSel selection = ps2_select_clamp((float)texture->Width,
                                                   (float)texture->Height,
                                                   ps2_projection_is_orthographic(),
                                                   ps2_texture_bound_is_tile_atlas(),
                                                   minU, minV, maxU, maxV);
    ps2_texture_state_apply_clamp_sel(selection.mode, selection.ufix, selection.vfix);
}

void ps2_texture_state_invalidate()
{
    s_clampMode = -1;
    s_regionUFix = -1;
    s_regionVFix = -1;
}

#endif // PS2_PLATFORM
