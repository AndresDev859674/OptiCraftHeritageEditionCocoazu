#ifdef PS2_PLATFORM

#include "ps2/render/Ps2Draw2D.h"
#include "ps2/render/Ps2DrawImmediate.h"

#include "ps2/render/Ps2ClipGuard.h"
#include "ps2/render/Ps2DrawCommon.h"
#include "ps2/render/Ps2GsQueue.h"
#include "ps2/render/Ps2MatrixStack.h"
#include "ps2/render/Ps2ProjectionState.h"
#include "ps2/render/Ps2RenderBackend.h"
#include "ps2/render/Ps2RenderContext.h"
#include "ps2/render/Ps2RenderGsState.h"
#include "ps2/render/Ps2RenderLighting.h"
#include "ps2/render/Ps2RenderStats.h"
#include "ps2/render/Ps2RenderTextureState.h"
#include "ps2/render/Ps2RenderTransform.h"
#include "ps2/render/Ps2RenderTypes.h"
#include "ps2/render/Ps2TextureGs.h"
#include "ps2/render/Ps2Tuning.h"
#include "ps2/render/Ps2Viewport.h"

#include <gsInline.h>
#include <gsKit.h>
#include <gsPrimitive.h>
#include <libvux.h>
#include <math.h>
#include <string.h>

namespace
{
constexpr unsigned int kPrimPoints = 0x0000;
constexpr unsigned int kPrimLines = 0x0001;
constexpr unsigned int kPrimLineLoop = 0x0002;
constexpr unsigned int kPrimLineStrip = 0x0003;
constexpr unsigned int kPrimTriangles = 0x0004;
constexpr unsigned int kPrimTriangleStrip = 0x0005;
constexpr unsigned int kPrimTriangleFan = 0x0006;
constexpr unsigned int kPrimQuads = 0x0007;
constexpr unsigned int kFaceCCW = 0x0901;
constexpr unsigned int kFaceBack = ps2RenderValue(Ps2RenderFace::Back);
constexpr int kOrthoBatchTriangles = 32;
}

typedef float Mat4[16];

extern GSGLOBAL* gsGlobal;
static Ps2RenderContext& st = ps2_render_context();

static inline VU_MATRIX mat4_to_vu(const Mat4 matrix)
{
    VU_MATRIX out;
    __builtin_memcpy(&out, matrix, 16 * sizeof(float));
    return out;
}

static inline float gsx(float x)
{
    if (!ps2_projection_is_orthographic())
        return x;
    return (x - ps2_projection_state().left) /
           (ps2_projection_state().right - ps2_projection_state().left) * ps2_viewport_width();
}

static inline float gsy(float y)
{
    if (!ps2_projection_is_orthographic())
        return y;
    return (1.0f - (y - ps2_projection_state().bottom) /
           (ps2_projection_state().top - ps2_projection_state().bottom)) * ps2_viewport_height();
}

static inline u64 mkcol(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    return GS_SETREG_RGBAQ(r, g, b, static_cast<u8>(a >> 1), 0);
}

static inline unsigned char clamp_colorf(float value)
{
    if (value <= 0.0f) return 0;
    if (value >= 1.0f) return 255;
    return static_cast<unsigned char>(value * 255.0f);
}

struct Ps2ArrayCursor
{
    const char* base;
    int stride;
};

static inline Ps2ArrayCursor vp_cursor()
{
    Ps2ArrayCursor cursor;
    cursor.base = static_cast<const char*>(st.vp);
    cursor.stride = st.vstride ? st.vstride : st.vsize * 4;
    return cursor;
}

static inline Ps2ArrayCursor tp_cursor()
{
    Ps2ArrayCursor cursor;
    cursor.base = static_cast<const char*>(st.tp);
    cursor.stride = st.tstride ? st.tstride : 2 * 4;
    return cursor;
}

static inline const float* at_(const Ps2ArrayCursor& cursor, int index)
{
    return reinterpret_cast<const float*>(cursor.base + index * cursor.stride);
}

static void getcol(int index, unsigned char& r, unsigned char& g,
                   unsigned char& b, unsigned char& a)
{
    if (!st.cen || !st.cp)
    {
        r = st.cr; g = st.cg; b = st.cb; a = st.ca;
        ps2_lighting_apply_vertex(index, r, g, b);
        ps2_lighting_apply_lightmap(r, g, b);
        return;
    }

    if (st.cfloat)
    {
        const int stride = st.cstride ? st.cstride : st.csize * 4;
        const float* color = reinterpret_cast<const float*>(
            static_cast<const char*>(st.cp) + index * stride);
        r = clamp_colorf(color[0]);
        g = clamp_colorf(color[1]);
        b = clamp_colorf(color[2]);
        a = st.csize >= 4 ? clamp_colorf(color[3]) : 255;
    }
    else
    {
        const int stride = st.cstride ? st.cstride : st.csize;
        const unsigned char* color = static_cast<const unsigned char*>(st.cp) + index * stride;
        r = color[0]; g = color[1]; b = color[2];
        a = st.csize >= 4 ? color[3] : 255;
    }

    ps2_lighting_apply_vertex(index, r, g, b);
    ps2_lighting_apply_lightmap(r, g, b);
}

static bool is_culled_3d(float x0, float y0, float x1, float y1, float x2, float y2)
{
    if (!st.cullFace || ps2_projection_is_orthographic())
        return false;

    const float area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
    if (fabsf(area) < 0.0001f)
        return false;

    const bool front = st.frontFace == kFaceCCW ? area < 0.0f : area > 0.0f;
    return st.cullMode == kFaceBack ? !front : front;
}

static inline int ps2_ortho_gs_z(float oz)
{
    if (!ps2_gs_state_depth_test_enabled())
    {
        PS2_DEPTH_STAT(ps2_render_stats().orthoZPinned++);
        return PS2_GS_Z_MAX;
    }

    const float range = ps2_projection_state().farPlane - ps2_projection_state().nearPlane;
    if (range == 0.0f)
    {
        PS2_DEPTH_STAT(ps2_render_stats().orthoZPinned++);
        return PS2_GS_Z_MAX;
    }

    const float zndc = (-2.0f * oz -
        (ps2_projection_state().farPlane + ps2_projection_state().nearPlane)) / range;
    float depth = (1.0f - zndc) * 0.5f;
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;
    const int z = ps2_gs_state_apply_polygon_depth_bias(
        static_cast<int>(depth * static_cast<float>(PS2_GS_Z_MAX)));
    PS2_DEPTH_STAT(ps2_render_stats().orthoZTested++;
                   if (z < ps2_render_stats().orthoZLow) ps2_render_stats().orthoZLow = z;
                   if (z > ps2_render_stats().orthoZHigh) ps2_render_stats().orthoZHigh = z;
                   if (oz < ps2_render_stats().orthoEyeZLow) ps2_render_stats().orthoEyeZLow = oz;
                   if (oz > ps2_render_stats().orthoEyeZHigh) ps2_render_stats().orthoEyeZHigh = oz;
                   ps2_render_stats().orthoNear = ps2_projection_state().nearPlane;
                   ps2_render_stats().orthoFar = ps2_projection_state().farPlane);
    return z;
}

static void project_cv(const ClipVert& vertex, float width, float height,
                       float& sx, float& sy, int& sz, float& q)
{
    q = 1.0f / vertex.cw;
    sx = ps2_clamp_guard((vertex.cx * q + 1.0f) * 0.5f * width, width);
    sy = ps2_clamp_guard((-vertex.cy * q + 1.0f) * 0.5f * height, height);
    if (ps2_transform_force_near_z_enabled())
    {
        sz = PS2_GS_Z_MAX;
        return;
    }
    sz = ps2_gs_state_apply_polygon_depth_bias(
        ps2_gs_depth(ps2_transform_depth_map(), vertex.cz, vertex.cw, q,
                     static_cast<float>(PS2_GS_Z_MAX)));
}

void ps2_draw_immediate_arrays(unsigned int mode, int first, int count, const float* preparedMvp) {
    if (!gsGlobal || !st.vp || count < 1) return;
    // Depth behaviour is driven entirely through the TEST/ZBUF registers
    // (ps2_apply_depth_state); keep the gsKit flag pinned ON so gsKit_clear
    // and any internal gsKit path stay consistent with the init-time setup.
    gsGlobal->ZBuffering = GS_SETTING_ON;
    if (!ps2_projection_is_orthographic())
    {
        PS2_RENDER_STAT(ps2_render_stats().draw3dCalls++);
#if MC_LOG_LEVEL > 2
        ++ps2_render_stats().profile3DrawCalls;
        ps2_render_stats().profile3Vertices += count;
#endif
    }

    // Reserve FIRST, then program state — see ps2_draw_apply_gs_state. Doing it
    // up front also means the exec+reset the guard may trigger cannot land
    // between two register writes meant to apply to the same primitive.
    // Barrier: staged 2D sprites were built under the state and packet
    // order that precede this draw, so they leave before it does.
    ps2_draw_2d_flush_pending();
    ps2_gs_queue_guard_prim(mode, count);
    ps2_gs_state_apply_draw_state();

    GSTEXTURE* tex = ps2_draw_resolve_texture(st.tp != nullptr && st.ten);
    const Ps2RenderState renderState = ps2_draw_capture_render_state(true, true);

    // Resolved once per draw call instead of per vertex/per triangle.
    const Ps2ArrayCursor vc = vp_cursor();
    const Ps2ArrayCursor tc = tp_cursor();
    const float texW = tex ? (float)tex->Width  : 1.0f;
    const float texH = tex ? (float)tex->Height : 1.0f;

    // Build MVP once per draw call for 3D mode.
    // Also build row-major VU_MATRIX copy for Vu0ApplyMatrix (VU0 macro mode HW transform).
    Mat4 mvp;
    VU_MATRIX vu_mvp;
    if (!ps2_projection_is_orthographic()) {
        if (preparedMvp)
			ps2_matrix_copy(mvp, preparedMvp);
        else
			ps2_matrix_multiply(mvp, ps2_matrix_projection(), ps2_matrix_model_view());
        vu_mvp = mat4_to_vu(mvp);
    }

    const float scrW = ps2_viewport_width(), scrH = ps2_viewport_height();
    const float gbx = ps2_guard_clip_scale(scrW);
    const float gby = ps2_guard_clip_scale(scrH);

    // Build clip-space vertex from array index i (3D only — do not call for ortho).
    auto make_cv = [&](int i) -> ClipVert {
        const float* v = at_(vc, i);
        VU_VECTOR vert __attribute__((aligned(16))) = {v[0], v[1], (st.vsize>=3?v[2]:0.0f), 1.0f};
        VU_VECTOR clip __attribute__((aligned(16)));
        Vu0ApplyMatrix(&vu_mvp, &vert, &clip);
        ClipVert cv;
        cv.cx = clip.x; cv.cy = clip.y; cv.cz = clip.z; cv.cw = clip.w;
        if (tex && st.tp && st.ten) { const float* t=at_(tc, i); cv.u=t[0]; cv.v=t[1]; }
        else { cv.u = cv.v = 0.0f; }
        getcol(i, cv.r, cv.g, cv.bl, cv.a);
        if (renderState.fogEnabled) {
            const float f = ps2_render_fog_factor(renderState, clip.w);
            const float inv = 1.0f - f;
            cv.r  = (u8)(f * cv.r  + inv * renderState.fogR * 255.0f);
            cv.g  = (u8)(f * cv.g  + inv * renderState.fogG * 255.0f);
            cv.bl = (u8)(f * cv.bl + inv * renderState.fogB * 255.0f);
        }
        return cv;
    };

    // Clip triangle against the near + guard-band planes (outcode trivial
    // accept/reject first) and submit all resulting triangles to GS.
    auto submit_clip_tri = [&](const ClipVert tri[3]) {
        int oc0 = ps2_clip_outcode(tri[0].cx, tri[0].cy, tri[0].cz, tri[0].cw, gbx, gby);
        int oc1 = ps2_clip_outcode(tri[1].cx, tri[1].cy, tri[1].cz, tri[1].cw, gbx, gby);
        int oc2 = ps2_clip_outcode(tri[2].cx, tri[2].cy, tri[2].cz, tri[2].cw, gbx, gby);
        if (oc0 & oc1 & oc2) { PS2_RENDER_STAT(ps2_render_stats().draw3dClipped++); return; }

        ClipVert poly[PS2_CLIP_MAX_POLY];
        poly[0] = tri[0]; poly[1] = tri[1]; poly[2] = tri[2];
        int n = 3;
        int mask = oc0 | oc1 | oc2;
        if (mask) {
            n = ps2_clip_poly_guard(poly, 3, mask, gbx, gby);
            if (n == 0) { PS2_RENDER_STAT(ps2_render_stats().draw3dClipped++); return; }
        }
        for (int k = 1; k + 1 < n; k++) {
            float x0,y0,x1,y1,x2,y2; float q0,q1,q2; int z0,z1,z2;
            project_cv(poly[0],   scrW, scrH, x0, y0, z0, q0);
            project_cv(poly[k],   scrW, scrH, x1, y1, z1, q1);
            project_cv(poly[k+1], scrW, scrH, x2, y2, z2, q2);
            if (ps2_tri_offscreen(x0,y0,x1,y1,x2,y2,scrW,scrH)) { PS2_RENDER_STAT(ps2_render_stats().draw3dOffscreen++); continue; }
            if (is_culled_3d(x0,y0,x1,y1,x2,y2)) { PS2_RENDER_STAT(ps2_render_stats().draw3dBackface++); continue; }
            PS2_RENDER_STAT(ps2_render_stats().draw3dPrims++);
            const ClipVert& c0=poly[0]; const ClipVert& c1=poly[k]; const ClipVert& c2=poly[k+1];
            const ClipVert& s0 = renderState.smoothShading ? c0 : c2;
            const ClipVert& s1 = renderState.smoothShading ? c1 : c2;
            if (tex) {
                ps2_texture_state_set_clamp_for_uv(tex,
                    c0.u*(float)tex->Width, c0.v*(float)tex->Height,
                    c1.u*(float)tex->Width, c1.v*(float)tex->Height,
                    c2.u*(float)tex->Width, c2.v*(float)tex->Height);
                GSPRIMSTQPOINT pts[3];
                pts[0].rgbaq=color_to_RGBAQ(PS2_TEXCOL(s0.r),PS2_TEXCOL(s0.g),PS2_TEXCOL(s0.bl),(u8)(s0.a>>1),q0); pts[0].stq=vertex_to_STQ(c0.u*q0,c0.v*q0); pts[0].xyz2=vertex_to_XYZ2(gsGlobal,x0,y0,z0);
                pts[1].rgbaq=color_to_RGBAQ(PS2_TEXCOL(s1.r),PS2_TEXCOL(s1.g),PS2_TEXCOL(s1.bl),(u8)(s1.a>>1),q1); pts[1].stq=vertex_to_STQ(c1.u*q1,c1.v*q1); pts[1].xyz2=vertex_to_XYZ2(gsGlobal,x1,y1,z1);
                pts[2].rgbaq=color_to_RGBAQ(PS2_TEXCOL(c2.r),PS2_TEXCOL(c2.g),PS2_TEXCOL(c2.bl),(u8)(c2.a>>1),q2); pts[2].stq=vertex_to_STQ(c2.u*q2,c2.v*q2); pts[2].xyz2=vertex_to_XYZ2(gsGlobal,x2,y2,z2);
                ps2_gs_queue_guard(3);
                gsKit_prim_list_triangle_goraud_texture_stq_3d(gsGlobal, tex, 3, pts);
            } else {
                ps2_gs_queue_guard(3);
                gsKit_prim_triangle_gouraud_3d(gsGlobal, x0,y0,z0, x1,y1,z1, x2,y2,z2,
                    mkcol(s0.r,s0.g,s0.bl,s0.a),
                    mkcol(s1.r,s1.g,s1.bl,s1.a),
                    mkcol(c2.r,c2.g,c2.bl,c2.a));
            }
        }
    };

    // Clip one 3D line segment and submit it as a GS LINE primitive.
    //
    // Lines get their own clipper rather than borrowing the polygon one -- see
    // ps2_clip_segment_guard in Ps2ClipGuard.h. They are also never textured:
    // gsKit's goraud line REGLIST carries only PRIM/RGBAQ/XYZ2, so the GS
    // rasterises them with TME off, which is exactly what the two callers
    // (the block selection box and the fishing line) ask for anyway.
    auto submit_clip_line = [&](ClipVert a, ClipVert b) {
        const int oca = ps2_clip_outcode(a.cx, a.cy, a.cz, a.cw, gbx, gby);
        const int ocb = ps2_clip_outcode(b.cx, b.cy, b.cz, b.cw, gbx, gby);
        if (oca & ocb) { PS2_RENDER_STAT(ps2_render_stats().draw3dClipped++); return; }

        const int mask = oca | ocb;
        if (mask && !ps2_clip_segment_guard(a, b, mask, gbx, gby)) {
            PS2_RENDER_STAT(ps2_render_stats().draw3dClipped++);
            return;
        }

        float x0, y0, x1, y1, q0, q1;
        int z0, z1;
        project_cv(a, scrW, scrH, x0, y0, z0, q0);
        project_cv(b, scrW, scrH, x1, y1, z1, q1);
        PS2_RENDER_STAT(ps2_render_stats().draw3dPrims++);
        ps2_gs_queue_guard(2);
        gsKit_prim_line_goraud_3d(gsGlobal, x0, y0, z0, x1, y1, z1,
            renderState.smoothShading ? mkcol(a.r, a.g, a.bl, a.a) : mkcol(b.r, b.g, b.bl, b.a),
            mkcol(b.r, b.g, b.bl, b.a));
    };

		if (ps2_try_draw_3d(mode, first, count, tex, mvp,
									false,
                                     nullptr, 0,
                                     nullptr, 0,
                                     st.vp, st.vstride, st.vsize,
                                 st.tp, st.tstride, st.ten,
                                 st.cp, st.cstride, st.csize, st.cen, st.cfloat,
								 ps2_draw_3d_fully_inside()))
        return;

    // Screen coords + GS depth for an ortho vertex.
    //
    // Ortho GUI still uses the MODELVIEW stack for translated/scaled/rotated
    // item icons and the inventory player preview. The old PS2 path ignored
    // MODELVIEW in ortho mode, so those models were submitted near 0,0 or
    // behind the inventory background.
    //
    // q is pinned to 1: there is no perspective divide in ortho, so the STQ
    // coordinates are the raw UVs. This used to be a combined 3D/ortho helper
    // whose 3D half no draw path could reach (the 3D modes all go through
    // make_cv/submit_clip_tri), plus a second, entirely unreferenced copy of
    // the same code.
    auto svq = [&](int i, float& sx, float& sy, int& sz) {
        const float* v = at_(vc, i);
        float ox, oy, oz, ow;
		ps2_matrix_transform_point(ps2_matrix_model_view(),
			v[0], v[1], (st.vsize >= 3 ? v[2] : 0.0f), ox, oy, oz, ow);
        if (fabsf(ow) > 1e-6f) { ox /= ow; oy /= ow; oz /= ow; }
        sx = gsx(ox); sy = gsy(oy); sz = ps2_ortho_gs_z(oz);
    };

    // ---- Ortho (GUI/HUD) emission ----
    //
    // One shared emitter for kPrimQuads / kPrimTriangles / kPrimTriangleStrip. These
    // were three verbatim copies of the same 30 lines, each submitting ONE
    // gsKit prim per triangle — and every such call writes its own GIF tag plus
    // a TEX0 register pair. Text is the worst case: FontRenderer draws one quad
    // per glyph, so a debug overlay or an open inventory was spending most of
    // its GS bandwidth on tags. Triangles are accumulated here and handed over
    // in one list, exactly like the terrain fast path does.
    //
    // Batching never reorders anything: it only merges consecutive triangles
    // inside a single glDrawArrays call, which already share one texture and
    // one blend state. The batch is flushed before any clamp change and before
    // returning, so nothing outlives the call.
    static GSPRIMSTQPOINT s_orthoBatch[kOrthoBatchTriangles * 3] __attribute__((aligned(16)));
    int orthoBatchCount = 0;                 // triangles staged
    bool orthoClampValid = false;
    Ps2ClampSel orthoClampLast = { -1, -1, -1 };

    auto flushOrthoBatch = [&]() {
        if (orthoBatchCount <= 0) return;
        ps2_gs_queue_guard(orthoBatchCount * 3);
        gsKit_prim_list_triangle_goraud_texture_stq_3d(
            gsGlobal, tex, orthoBatchCount * 3, s_orthoBatch);
        orthoBatchCount = 0;
    };

    // Ortho triangle from three array indices. svq always succeeds here (no
    // near plane in ortho), so there is nothing to clip.
    auto emit_ortho_tri = [&](int i0, int i1, int i2) {
        float x0,y0,x1,y1,x2,y2;
        int z0, z1, z2;
        svq(i0,x0,y0,z0); svq(i1,x1,y1,z1); svq(i2,x2,y2,z2);

        unsigned char r0,g0,b0_,a0, r1,g1,b1_,a1, r2,g2,b2_,a2;
        getcol(i0,r0,g0,b0_,a0); getcol(i1,r1,g1,b1_,a1); getcol(i2,r2,g2,b2_,a2);
        if (!renderState.smoothShading) {
            r0 = r1 = r2; g0 = g1 = g2; b0_ = b1_ = b2_; a0 = a1 = a2;
        }

        if (!tex) {
            ps2_gs_queue_guard(3);
            gsKit_prim_triangle_gouraud_3d(gsGlobal,
                x0,y0,z0, x1,y1,z1, x2,y2,z2,
                mkcol(r0,g0,b0_,a0), mkcol(r1,g1,b1_,a1), mkcol(r2,g2,b2_,a2));
            return;
        }

        const float* t0 = at_(tc, i0);
        const float* t1 = at_(tc, i1);
        const float* t2 = at_(tc, i2);

        // gsKit_set_clamp writes into the DMA stream immediately, so anything
        // already staged must go out under the clamp it was built with.
        float minU, minV, maxU, maxV;
        ps2_uv_bounds3(t0[0]*texW, t0[1]*texH,
                       t1[0]*texW, t1[1]*texH,
                       t2[0]*texW, t2[1]*texH,
                       minU, minV, maxU, maxV);
        const Ps2ClampSel sel = ps2_select_clamp(texW, texH, true, false,
                                                 minU, minV, maxU, maxV);
        if (!orthoClampValid || sel != orthoClampLast) {
            flushOrthoBatch();
            ps2_texture_state_apply_clamp_sel(sel.mode, sel.ufix, sel.vfix);
            orthoClampLast = sel;
            orthoClampValid = true;
        }

        if (orthoBatchCount == kOrthoBatchTriangles)
            flushOrthoBatch();

        // q = 1 throughout: ortho has no perspective divide, so STQ is the raw UV.
        GSPRIMSTQPOINT* p = &s_orthoBatch[orthoBatchCount * 3];
        p[0].rgbaq = color_to_RGBAQ(PS2_TEXCOL_GUI(r0),PS2_TEXCOL_GUI(g0),PS2_TEXCOL_GUI(b0_),(u8)(a0>>1),1.0f);
        p[0].stq   = vertex_to_STQ(t0[0], t0[1]);
        p[0].xyz2  = vertex_to_XYZ2(gsGlobal, x0, y0, z0);
        p[1].rgbaq = color_to_RGBAQ(PS2_TEXCOL_GUI(r1),PS2_TEXCOL_GUI(g1),PS2_TEXCOL_GUI(b1_),(u8)(a1>>1),1.0f);
        p[1].stq   = vertex_to_STQ(t1[0], t1[1]);
        p[1].xyz2  = vertex_to_XYZ2(gsGlobal, x1, y1, z1);
        p[2].rgbaq = color_to_RGBAQ(PS2_TEXCOL_GUI(r2),PS2_TEXCOL_GUI(g2),PS2_TEXCOL_GUI(b2_),(u8)(a2>>1),1.0f);
        p[2].stq   = vertex_to_STQ(t2[0], t2[1]);
        p[2].xyz2  = vertex_to_XYZ2(gsGlobal, x2, y2, z2);
        orthoBatchCount++;
    };

    // Ortho lines. No texture and no batch: a LINE carries a different PRIM
    // than the staged triangle list, so it could not join that packet anyway,
    // and a draw call is all one mode -- a line call never has triangles
    // waiting behind it.
    auto emit_ortho_line = [&](int i0, int i1) {
        float x0, y0, x1, y1;
        int z0, z1;
        svq(i0, x0, y0, z0); svq(i1, x1, y1, z1);

        unsigned char r0,g0,b0_,a0, r1,g1,b1_,a1;
        getcol(i0, r0,g0,b0_,a0); getcol(i1, r1,g1,b1_,a1);
        if (!renderState.smoothShading) { r0 = r1; g0 = g1; b0_ = b1_; a0 = a1; }

        ps2_gs_queue_guard(2);
        gsKit_prim_line_goraud_3d(gsGlobal, x0, y0, z0, x1, y1, z1,
            mkcol(r0,g0,b0_,a0), mkcol(r1,g1,b1_,a1));
    };

    // One triangle from three array indices, whichever mode we are in. The
    // ortho/3D branch used to be written out in all three loops below.
    // Deliberately NOT used by the kPrimQuads 3D case: a quad shares two corners
    // between its triangles, and routing it through here would transform six
    // vertices where four suffice — 50% more VU0 work on the hottest path in
    // the game.
    auto emit_tri = [&](int i0, int i1, int i2) {
        if (ps2_projection_is_orthographic()) { emit_ortho_tri(i0, i1, i2); return; }
        ClipVert tri[3] = { make_cv(i0), make_cv(i1), make_cv(i2) };
        submit_clip_tri(tri);
    };

    auto emit_line = [&](int i0, int i1) {
        if (ps2_projection_is_orthographic()) { emit_ortho_line(i0, i1); return; }
        submit_clip_line(make_cv(i0), make_cv(i1));
    };

    auto emit_point = [&](int i) {
        unsigned char r, g, b, a;
        getcol(i, r, g, b, a);
        float x, y;
        int z;
        if (ps2_projection_is_orthographic()) {
            svq(i, x, y, z);
        } else {
            ClipVert cv = make_cv(i);
            const int oc = ps2_clip_outcode(cv.cx, cv.cy, cv.cz, cv.cw, gbx, gby);
            if (oc != 0) { PS2_RENDER_STAT(ps2_render_stats().draw3dClipped++); return; }
            float q;
            project_cv(cv, scrW, scrH, x, y, z, q);
            if (x < 0.0f || y < 0.0f || x >= scrW || y >= scrH) {
                PS2_RENDER_STAT(ps2_render_stats().draw3dOffscreen++);
                return;
            }
        }
        ps2_gs_queue_guard(1);
        gsKit_prim_point(gsGlobal, x, y, z, mkcol(r, g, b, a));
    };

    if (mode == kPrimPoints) {
        for (int i = 0; i < count; ++i)
            emit_point(first + i);
    } else if (mode == kPrimQuads) {
        const int nq = count / 4;
        for (int qi = 0; qi < nq; qi++) {
            const int b = first + qi*4;
            if (!ps2_projection_is_orthographic()) {
                // 3D: four transforms, not six — v0 and v2 are shared. Each
                // sub-triangle is still clipped independently against the near
                // plane.
                ClipVert v0=make_cv(b+0), v1=make_cv(b+1), v2=make_cv(b+2), v3=make_cv(b+3);
                ClipVert tri1[3]={v0,v1,v2}; submit_clip_tri(tri1);
                ClipVert tri2[3]={v0,v2,v3}; submit_clip_tri(tri2);
            } else {
                // Two explicit triangles, not gsKit_prim_quad_*: that helper
                // emits a TRISTRIP, so it expects the "Z" corner order
                // (TL,TR,BL,BR). kPrimQuads hands us perimeter order, and a strip
                // over perimeter corners draws (0,1,2)+(1,2,3), which leaves one
                // wedge of the rectangle unfilled and doubles another — every
                // untextured GUI rect would render bowtied.
                emit_ortho_tri(b+0, b+1, b+2);
                emit_ortho_tri(b+0, b+2, b+3);
            }
        }
    } else if (mode == kPrimTriangles) {
        const int nt = count / 3;
        for (int t = 0; t < nt; t++) {
            const int b = first + t*3;
            emit_tri(b+0, b+1, b+2);
        }
    } else if (mode == kPrimTriangleStrip) {
        for (int i = 0; i + 2 < count; i++) {
            // Odd triangles swap the first two indices to keep winding.
            const int b0 = first + (i & 1 ? i+1 : i);
            const int b1 = first + (i & 1 ? i   : i+1);
            emit_tri(b0, b1, first + i + 2);
        }
    } else if (mode == kPrimTriangleFan) {
        // The sunrise/sunset glow in RenderGlobal::renderSky is the game's only
        // fan, and it was the one primitive no PS2 path handled: the mode fell
        // through this switch and the draw silently produced nothing.
        for (int i = 1; i + 1 < count; i++)
            emit_tri(first, first + i, first + i + 1);
    } else if (mode == kPrimLines || mode == kPrimLineStrip || mode == kPrimLineLoop) {
        // The block selection box (kPrimLines + kPrimLineStrip) and the fishing
        // line (kPrimLineStrip) were in the same position as the fan above.
        // kPrimLines consumes disjoint pairs; the strip/loop forms chain. An odd
        // trailing vertex in a kPrimLines batch has no partner and is dropped,
        // which is what desktop GL does with it too.
        const int step = (mode == kPrimLines) ? 2 : 1;
        for (int i = 0; i + 1 < count; i += step)
            emit_line(first + i, first + i + 1);
        // kPrimLineLoop closes back onto the first vertex.
        if (mode == kPrimLineLoop && count > 2)
            emit_line(first + count - 1, first);
    }

    flushOrthoBatch();
}



#endif // PS2_PLATFORM
