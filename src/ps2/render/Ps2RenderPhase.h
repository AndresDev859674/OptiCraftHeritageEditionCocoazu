#pragma once

#ifdef PS2_PLATFORM

// Coarse attribution of the render frame.
//
// "render avg" in the FRAME log is the whole of EntityRenderer::updateCameraAndRender,
// and the fast-draw counters only cover the three inner blocks of the terrain
// path (transform, project, packet build). Measured 2026-07-28 those three summed
// to 7.7ms against a render avg of 23.3ms, so two thirds of the render frame was
// unattributed and could not be reasoned about. These brackets close that gap by
// naming the phases updateCameraAndRender actually runs.
//
// Counted in EE cycles from COP0 Count (one tick per CPU cycle at ~294MHz, wraps
// every ~14.6s, which is far longer than the log period). Reading it is a single
// instruction, so bracketing a phase is free at this granularity.

enum Ps2RenderPhase
{
    PS2_RPHASE_SKY = 0,     // renderSky
    PS2_RPHASE_FRUSTUM,     // clipRenderersByFrustrum
    PS2_RPHASE_BUILD,       // updateRenderers -> chunk meshing steps
    PS2_RPHASE_PASS0,       // sortAndRender pass 0 (opaque terrain)
    PS2_RPHASE_ENTITIES,    // renderEntities
    PS2_RPHASE_PASS1,       // sortAndRender pass 1 (water/ice/glass)
    PS2_RPHASE_HAND,        // renderHand -> held item + overlays
    PS2_RPHASE_HUD,         // GuiIngame::renderGameOverlay
    // Nested inside PS2_RPHASE_ENTITIES rather than disjoint from it. ENT_DRAW
    // sums the per-entity RenderManager::renderEntity calls, TILE_DRAW the
    // tile-entity loop; ENTITIES minus both is the list scan, the range and
    // frustum tests and the per-entity blockExists probe.
    //
    // Measured 2026-09-05 in the tutorial world: ENTITIES was 19.5ms of a
    // 93.3ms frame for 48 entities, while the fast-draw cycle counters
    // (xform/project/emit, Ps2RenderStats) accounted for about 5ms of it. The
    // remaining two thirds had no name, which is why sizing an entity VU1 path
    // against it was guesswork.
    //
    // Read their "max" differently from every phase above: those are bracketed
    // once per frame, so max is the worst frame. These two are bracketed once
    // per entity, so max is the worst single entity and can legitimately print
    // below the per-frame average (entDraw=14.9/max2.9 means fifteen
    // milliseconds spread over many entities, none costing more than three).
    PS2_RPHASE_ENT_DRAW,
    PS2_RPHASE_TILE_DRAW,
    // Nested inside PS2_RPHASE_HUD, the same way ENT_DRAW/TILE_DRAW nest inside
    // ENTITIES. HUD measured a flat 12.6ms of a 24.4ms render on 2026-09-06 --
    // avg and max within 0.1ms of each other across a whole session, with the
    // world both empty and full -- so whatever it is does the same work every
    // frame regardless of the scene. That rules out fill and terrain and leaves
    // the fixed GUI content, but not which part of it:
    //   HUD_ITEMS  the nine hotbar slots, each a 3D block through RenderBlocks
    //   HUD_TEXT   chat lines and the player list
    //   HUD_HINTS  LegacyControlTooltipHud -- five prompts, ten shadowed string
    //              draws, ~150 glyph quads, rebuilt from scratch every frame
    // HUD minus the three is the overlay/frame/crosshair/status setup.
    PS2_RPHASE_HUD_ITEMS,
    PS2_RPHASE_HUD_TEXT,
    PS2_RPHASE_HUD_HINTS,
    PS2_RPHASE_COUNT
};

extern "C" void ps2_perf_add_render_phase(int phase, unsigned int cycles);

static inline unsigned int ps2_rphase_cycles(void)
{
    unsigned int c;
    __asm__ __volatile__("mfc0 %0, $9" : "=r"(c));
    return c;
}

// Bracket a phase. Declares a scoped start value, so two brackets in the same
// scope need different names.
#define PS2_RPHASE_BEGIN(v)        const unsigned int v = ps2_rphase_cycles()
#define PS2_RPHASE_END(v, phase)   ps2_perf_add_render_phase((phase), ps2_rphase_cycles() - (v))

#endif // PS2_PLATFORM
