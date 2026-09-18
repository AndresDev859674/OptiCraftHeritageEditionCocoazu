#pragma once

// -----------------------------------------------------------------------------
// Frame pacing (PLATFORM_MESH_BUDGET)
// -----------------------------------------------------------------------------

// Bound chunk meshing to a slice of each frame.
//
// Without this the Wii took the desktop path, which is unbounded in TWO places
// at once, and they compound:
//
//   * RenderGlobal::updateRenderers builds EVERY queued renderer within 16
//     blocks of the player in one go. The candidate cap next to it was gated on
//     PLATFORM_CONSOLE_LOW, which is 0 here.
//   * EntityRenderer then calls updateRenderers again, and again, in a do/while
//     that only stops when the queue drains or a wall-clock limit passes.
//
// So walking into new terrain, or any edit that dirties a handful of sections,
// spent as much of the frame as it needed. That is the hitch, and it is why the
// budget and the retry loop are one knob: bounding the inner call while the
// outer loop still retries measures well and changes nothing.
//
// The PS2's partial-build state machine is NOT part of this. There,
// updateRenderer() is one step of a section and can return having meshed
// nothing, which is why MeshBudget::run still has to ask ps2LastStepDidWork()
// before charging -- inside a #if PLATFORM_PS2, and that is the only line of the
// budget that is platform-specific. Here the call meshes a whole section or is
// never made, so the budget is just "how many, and for how long". That is why
// this could be turned on without porting the capture-slot machinery this file
// used to point at as the blocker.
#undef  PLATFORM_MESH_BUDGET
#define PLATFORM_MESH_BUDGET                     1

// Presentation cap, in frames per second. 0 = one VIDEO_WaitVSync per frame,
// i.e. whatever the CPU manages up to the 60 Hz (50 Hz PAL) retrace.
//
// 30 for the same reason the PS2 runs PS2_TARGET_FPS 30: a loaded session
// does not hold 60 anyway (the memory log's avgFps 5-7 under streaming, and
// 20-40 in a settled world), and a rate that swings between 25 and 60 reads
// worse than a flat 30. What the cap buys is not the frames it drops but the
// retrace it waits through: the main thread sleeps in VIDEO_WaitVSync() for
// the second half of every frame, and that is exactly the CPU the async
// generation worker (PLATFORM_WII_ASYNC_GENERATION_THREAD_PRIORITY) runs on.
// wiigl_end_frame() paces round(refresh / target) retraces between presents:
// 2 on NTSC/PAL60, 2 on PAL50 (25 fps). The count runs from the previous
// present, so a frame that misses the first retrace presents on the second
// instead of being held for a third.
#define WII_TARGET_FPS                           30

// 5 ms of a 33 ms frame is the mesh governor; the count is the backstop for
// when the clock misbehaves. Sized for WII_TARGET_FPS 30 -- at 60 these were
// 3 ms / 6 of a 16.7 ms frame, and halving the frame count without raising
// the per-frame budget would have halved how fast terrain fills in. The
// Broadway meshes a section far faster than the EE, so if the FPS counter
// says there is headroom, raise CHUNK_BUILD_BUDGET_MS first and watch how
// fast terrain fills in behind you.
#undef  PLATFORM_MAX_RENDERER_UPDATES_PER_FRAME
#define PLATFORM_MAX_RENDERER_UPDATES_PER_FRAME  8
// Edit-latency lane; see PS2_URGENT_MESH_DISTANCE_SQ. The Wii has no staging
// pool, so the lease reserve stays 0.
#undef  PLATFORM_URGENT_MESH_DISTANCE_SQ
#define PLATFORM_URGENT_MESH_DISTANCE_SQ         1024.0f
// 20 ms guaranteed a lost retrace on every block edit (build worst 19-25 ms in
// the 2026-09 perf log against a 33 ms period that also carries tick, HUD and
// entities). The incremental build below already spreads a section over
// frames, so a smaller slice only delays the edit by a frame or two.
#undef  PLATFORM_URGENT_MESH_BUDGET_MS
#define PLATFORM_URGENT_MESH_BUDGET_MS           10
#undef  PLATFORM_CHUNK_BUILD_BUDGET_MS
#define PLATFORM_CHUNK_BUILD_BUDGET_MS           5

// Incremental terrain meshing. A Wii section no longer compiles all 4096 blocks
// in one updateRenderer() call. The block count is the deterministic fallback;
// the microsecond ceiling trims unusually expensive AO/liquid steps.
#define PLATFORM_WII_CHUNK_BUILD_BLOCKS_PER_STEP 256
#define PLATFORM_WII_CHUNK_BUILD_STEP_US         2500

// Mesh warm-up after entering a local world. RenderGlobal creates its whole
// renderer grid at the end of Minecraft::changeWorld(); without a short loading
// phase the first gameplay frames have to compile that grid while also running
// ticks and drawing the world. The minimum is only enforced for normal local
// world entry; portal transitions can leave as soon as their terrain is ready.
// Keep the warm-up finite so dense saves fall back to normal streaming.
#define PLATFORM_WII_LOAD_MIN_MS                  1200
#define PLATFORM_WII_LOAD_WARMUP_MS               2500

// GX display-list cache maintenance. At 10MB of an 11MB live-list budget,
// evict at most two non-visible sections farther than the keep radius each
// frame. This leaves room for the newly exposed terrain without a visible
// mass clear -- and, just as importantly, gives a player-triggered edit
// (block break/place) somewhere to find free budget for its atomic
// staging-list rebuild (see WorldRenderer::updateRenderer()) without having
// to wait for the player to walk and reposition the renderer grid.
//
// TINY's own grid only reaches ~32-40 blocks from the player (5 chunks wide,
// +-2 chunks from centre; see PLATFORM_CHUNK_CACHE_RADIUS below), and TINY's
// steady-state list bytes already sit at roughly this budget's ceiling (see
// the estimate above PLATFORM_DEFAULT_RENDER_DISTANCE). A 32-block keep
// radius left only the single outermost chunk ring eligible for eviction, and
// only the part of that ring currently behind the camera -- standing still
// after an edit, that pool is often empty, so the rebuild stays queued until
// something else (player movement) frees memory elsewhere. 16 blocks (one
// chunk) opens a real ring of far/off-screen candidates at TINY while still
// keeping everything adjacent to the player off-limits.
#define PLATFORM_WII_GXLIST_EVICT_HIGH_WATER_BYTES (10u * 1024u * 1024u)
#define PLATFORM_WII_GXLIST_EVICT_KEEP_RADIUS_BLOCKS 16.0f
#define PLATFORM_WII_GXLIST_EVICT_MAX_PER_FRAME  2

// Candidate cap. Higher than the executed budget on purpose: candidates that
// bail out on the generation gate cost almost nothing, and starving the list
// would leave sections queued behind renderers that are not ready.
#undef  PLATFORM_RENDERER_UPDATE_CANDIDATES_PER_FRAME
#define PLATFORM_RENDERER_UPDATE_CANDIDATES_PER_FRAME 12

// Autosave period, in world ticks. Vanilla is 40 (2 s).
//
// Every firing deflates up to two dirty chunks synchronously inside a world
// tick. Wii defers the much slower level.dat and map metadata writes to full
// saves through PLATFORM_RUNTIME_AUTOSAVE_LEVEL_DATA below.
//
// 1200 ticks is 60 seconds at vanilla's 20 TPS -- but a loaded Wii session
// doesn't sustain 20 TPS (2026-08-12 log: avgFps 5-7, capped at 2 catch-up
// ticks/frame puts the real tick rate around 12-14/s), so this lands closer
// to 1.5-2 real minutes between firings in practice. That is a real trade and
// it is the reason this is a named knob rather than a new hardcoded number: a
// crash or a pulled power cord now costs up to that much play instead of 2s.
// The PS2 answer to the same problem was PS2_DISABLE_RUNTIME_AUTOSAVE -- no
// autosave at all, save from the menu -- which is worse here, because libfat
// can absorb a periodic save and a Memory Card genuinely cannot. Lower this
// if the console is being switched off abruptly; raise it further if the
// hitch (see wii_perf_add_saveworldinfo_ns/wii_perf_add_mapstorage_ns in
// World::saveLevel, Minecraft.cpp) still lands often enough to notice.
//
// This only changes how OFTEN saveLevel() fires, not how long it takes --
// that same 2026-08-12 log still showed a ~1.7-1.9s full-tick freeze on the
// firings that included metadata (World.cpp's saveLevel() had no timer of its
// own until this session, so the cost was invisible; unloadSave, the per-chunk
// save path, stayed under 70ms in the same window). Was 600 (30s nominal).
#undef  PLATFORM_AUTOSAVE_PERIOD_TICKS
#define PLATFORM_AUTOSAVE_PERIOD_TICKS           1200
// Keep each synchronous Wii autosave batch small; desktop retains the vanilla 24.
#undef  PLATFORM_INCREMENTAL_CHUNK_SAVE_LIMIT
#define PLATFORM_INCREMENTAL_CHUNK_SAVE_LIMIT      2

// Runtime chunk writes are already bounded to two dirty chunks per autosave,
// but saveLevel() also rewrites level.dat and all dirty map data synchronously.
// Those metadata writes measured at 1.7-1.9 seconds on libfat. Keep incremental
// chunk persistence during play and defer metadata/player persistence to the
// existing full save paths used by the pause menu, world change, and shutdown.
#undef  PLATFORM_RUNTIME_AUTOSAVE_LEVEL_DATA
#define PLATFORM_RUNTIME_AUTOSAVE_LEVEL_DATA      0
