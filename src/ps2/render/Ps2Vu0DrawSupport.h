#pragma once

#ifdef PS2_PLATFORM

#include <gsKit.h>
#include <gsTexture.h>
#include <libvux.h>

#include "ps2/render/Ps2ClipGuard.h"

#define PS2_VU0_BATCH_SIZE 64
#define PS2_VU0_STRIP_QUADS 1
#define PS2_VU0_STRIP_MAX_VERTS (32 * 4)

#ifdef PS2_RENDER_STATS
inline unsigned int ps2_vu0_ee_cycles()
{
    unsigned int cycles;
    __asm__ __volatile__("mfc0 %0, $9" : "=r"(cycles));
    return cycles;
}
#define PS2_VU0_CYC_BEGIN(v) const unsigned int v = ps2_vu0_ee_cycles()
#define PS2_VU0_CYC_END(v, acc) do { (acc) += ps2_vu0_ee_cycles() - (v); } while (0)
#else
#define PS2_VU0_CYC_BEGIN(v) do { } while (0)
#define PS2_VU0_CYC_END(v, acc) do { } while (0)
#endif

struct Ps2ProjVert
{
    float x;
    float y;
    float q;
    float w;
    int z;
};

struct Ps2EmitVert
{
    float u;
    float v;
    unsigned char r;
    unsigned char g;
    unsigned char bl;
    unsigned char a;
};

void ps2_vu0_queue_guard(GSGLOBAL* gs, int vertexCount);
VU_MATRIX ps2_vu0_mvp_to_vu(const float* matrix);
void ps2_vu0_project(const Ps2DepthMap& depth,
                     bool forceNearZ,
                     float cx,
                     float cy,
                     float cz,
                     float cw,
                     float halfWidth,
                     float halfHeight,
                     Ps2ProjVert& out);
bool ps2_vu0_cull(bool cullFace,
                  bool ccw,
                  bool back,
                  float x0,
                  float y0,
                  float x1,
                  float y1,
                  float x2,
                  float y2);
u64 ps2_vu0_clamp_reg(const Ps2ClampSel& selection);
u64 ps2_vu0_tex0(const GSGLOBAL* gs, const GSTEXTURE* texture);
GSPRIMSTQPOINT* ps2_vu0_triangle_batch();
GSPRIMSTQPOINT* ps2_vu0_strip_batch();
Ps2ClampSel* ps2_vu0_strip_clamp();

#endif
