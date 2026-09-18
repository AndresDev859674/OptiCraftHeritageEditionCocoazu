#include "platform/WorkProfiler.h"
#include "platform/Log.h"
#include "ps2/render/Ps2TextureGs.h"
#include "ps2/render/Ps2Draw2D.h"

#ifdef PS2_PLATFORM

#include <gsKit.h>
#include <gsTexture.h>
#include <gsInline.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "ps2/render/Ps2Graphics.h"
#include "ps2/render/Ps2Tuning.h" // PS2_TEXTURE_PARTIAL_UPLOAD, PS2_TERRAIN_MIPMAPS
#include "net/minecraft/src/legacy/LegacyLook.h"

// Mip levels ride the PSMT8 path only: they index the level-0 CLUT, so the
// CT16 build has no palette to share and keeps rejecting level > 0.
#if defined(PS2_ENABLE_PSMT8) && PS2_TERRAIN_MIPMAPS
#define PS2_TEX_MIPS 1
#else
#define PS2_TEX_MIPS 0
#endif

// The bound name. This is render state rather than a texture property, but it
// belongs to whoever owns the table: the GS draw paths ask this module which
// texture to rasterise with, and the OpenGL compatibility layer forwards
// glBindTexture here instead of keeping a second copy that could drift.
static unsigned int s_boundTex = 0;
static bool s_legacyWorldPass = false;

// GS PSM_CT16: 16-bit A1B5G5R5 — [15]=A, [14:10]=B, [9:5]=G, [4:0]=R.
// Halves VRAM vs CT32 (the GS only has 4 MB). Alpha drops to 1 bit, fine for
// the alpha-tested terrain cutouts; GUI translucency degrades slightly.
static inline u16 rgba_to_psmct16(const u8* src) {
    u16 r = (u16)(src[0] >> 3);
    u16 g = (u16)(src[1] >> 3);
    u16 b = (u16)(src[2] >> 3);
    u16 a = (u16)(src[3] >= 128 ? 1 : 0);
    return (u16)((a << 15) | (b << 10) | (g << 5) | r);
}

#ifdef PS2_ENABLE_PSMT8
// ---- PSMT8 palettization ----
//
// Game textures are stored as 8-bit palette indices + a 256-entry CT16 CLUT:
// half the VRAM and EE RAM of CT16, and half the DMA per animated-texture
// re-upload. The CLUT keeps the exact same A1B5G5R5 colors CT16 had, so any
// texture with <= 256 unique CT16 colors converts losslessly; richer textures
// (the prerendered gui_blocks atlas) quantize to their 256 most popular CT16
// colors with nearest-match remapping.

// The GS reads an 8-bit CSM1 CLUT with index bits 3 and 4 swapped (entries
// arranged as 8-column blocks). Store entry i at this position so TEX0 lookups
// hit the right color.
static inline int ps2_clut_csm1_pos(int i) {
    return (i & ~0x18) | ((i & 0x08) << 1) | ((i & 0x10) >> 1);
}

// Squared distance between two CT16 colors; alpha-bit mismatch is penalized
// so cutout pixels never snap to an opaque color (and vice versa).
static inline int ps2_ct16_dist(u16 a, u16 b) {
    int dr = (int)(a & 31)         - (int)(b & 31);
    int dg = (int)((a >> 5) & 31)  - (int)((b >> 5) & 31);
    int db = (int)((a >> 10) & 31) - (int)((b >> 10) & 31);
    int da = ((a ^ b) >> 15) & 1;
    return dr*dr + dg*dg + db*db + da * 0x10000;
}

static inline u16 ps2_legacy_grade_ct16(u16 color) {
    const u16 alpha = color & 0x8000u;
    const float r = legacyLookChannel(static_cast<float>(color & 31u) / 31.0f);
    const float g = legacyLookChannel(static_cast<float>((color >> 5) & 31u) / 31.0f);
    const float b = legacyLookChannel(static_cast<float>((color >> 10) & 31u) / 31.0f);
    const u16 r5 = static_cast<u16>(r * 31.0f + 0.5f);
    const u16 g5 = static_cast<u16>(g * 31.0f + 0.5f);
    const u16 b5 = static_cast<u16>(b * 31.0f + 0.5f);
    return static_cast<u16>(alpha | (b5 << 10) | (g5 << 5) | r5);
}

static void ps2_build_legacy_clut(const u16* normalClut, u16* legacyClut) {
    for (int i = 0; i < 256; ++i)
        legacyClut[i] = ps2_legacy_grade_ct16(normalClut[i]);
}

// Build a <=256-color palette for npx CT16 pixels and fill idx[] with the
// palette index of every pixel. palette[] receives the entries in linear
// (unswizzled) order; returns the palette size.
static int ps2_palettize_ct16(const u16* px, u32 npx, u8* idx, u16* palette) {
    PlatformLoadWorkScope paletteWork(PlatformLoadWork::Palette);
    // CT16 is only 16 bits, so an exact histogram over all 65536 values is
    // cheap (256 KB, transient) and avoids any hashing.
    u32* hist = (u32*)calloc(65536, sizeof(u32));
    u16* lut  = (u16*)malloc(65536 * sizeof(u16)); // CT16 value -> palette index
    if (!hist || !lut) {
        if (hist) free(hist);
        if (lut) free(lut);
        return 0;
    }
    for (u32 p = 0; p < npx; p++)
        hist[px[p]]++;

    int nPal = 0;
    // First pass: take colors while they fit (covers the lossless case).
    for (int v = 0; v < 65536; v++) {
        if (!hist[v]) continue;
        if (nPal < 256) {
            lut[v] = (u16)nPal;
            palette[nPal++] = (u16)v;
        } else { nPal = 257; break; }
    }

    if (nPal > 256) {
        // Reuse the remap buffer as a sparse color list until selection ends.
        int colorCount = 0;
        for (int v = 0; v < 65536; v++) {
            if (hist[v]) lut[colorCount++] = (u16)v;
        }
        // Equal counts retain the original ascending CT16 tie order.
        std::partial_sort(lut, lut + 256, lut + colorCount,
            [hist](u16 a, u16 b) {
                return hist[a] != hist[b] ? hist[a] > hist[b] : a < b;
            });
        nPal = 256;
        // Copy every selected color before reusing lut as a color-index map.
        for (int k = 0; k < nPal; k++)
            palette[k] = lut[k];
        for (int k = 0; k < nPal; k++) {
            hist[palette[k]] = 0;
            lut[palette[k]] = (u16)k;
        }
        // Remap every remaining (dropped) color to its nearest palette entry.
        for (int v = 0; v < 65536; v++) {
            if (!hist[v]) continue;     // not present or already in palette
            int best = 0, bestD = 0x7FFFFFFF;
            for (int k = 0; k < nPal; k++) {
                int d = ps2_ct16_dist((u16)v, palette[k]);
                if (d < bestD) { bestD = d; best = k; }
            }
            lut[v] = (u16)best;
        }
    }

    for (u32 p = 0; p < npx; p++)
        idx[p] = (u8)lut[px[p]];

    free(hist);
    free(lut);
    return nPal;
}
#endif // PS2_ENABLE_PSMT8

// ---- Texture table ----

struct PS2Tex {
    GSTEXTURE gs;
    u16*      cpuMem;  // CT16 path: pixel copy for glTexSubImage2D; 64-byte aligned for GS DMA
#ifdef PS2_ENABLE_PSMT8
    u8*       cpuIdx;  // T8 path: 8-bit palette indices (this is gs.Mem at upload time)
    u16*      clut;    // T8 path: 256 CT16 entries, CSM1-swizzled, 64-byte aligned
    u16*      legacyClut;
    u16*      remap;   // T8 path: lazy CT16->index memo for glTexSubImage2D (0xFFFF = empty)
    u32       normalVramClut;
    u32       legacyVramClut;
    u32       clutVramSize;
    bool      dirtyLegacyClut;
#endif
    u32       vramSize; // bytes reserved in GS VRAM (for the reuse free-list)
    bool      valid;
    bool      uploaded;
    bool      tileAtlas;
    bool      dirtyUpload; // sub-image touched the cpu copy; re-send on next bind
    // Which GS page rows that traffic covered, one bit per 64 texel rows. Zero
    // while dirtyUpload is set means "re-send all of it" -- what an unsupported
    // geometry leaves behind, and the state a fresh texture starts in.
    u32       dirtyPageRows;
#if PS2_TEX_MIPS
    // Levels 1..N of a tile atlas, index level-1. Each is its own VRAM block
    // (MIPTBP1 addresses them independently, so they need not follow level 0)
    // and its own index copy, sampled through the level-0 CLUT.
    struct MipLevel {
        u8* idx;
        u32 vram;
        u32 vramSize;
        u32 width;
        u32 height;
        u32 tbw;
    };
    MipLevel  mip[PS2_TERRAIN_MIP_MAX_LEVELS];
    int       mipLevels;      // resident levels beyond 0; TEX1.MXL
    u32       dirtyMipLevels; // bit level-1: index copy changed, whole level re-send pending
#endif
};
static PS2Tex s_tex[PS2_MAX_TEX];
#if PS2_TEX_MIPS
// ps2_texture_sampler_registers() recovers the table entry from the GSTEXTURE
// pointer the draw paths carry, which only works while gs is the first member.
static_assert(offsetof(PS2Tex, gs) == 0, "PS2Tex::gs must stay the first member");
#endif

// ---- GS VRAM free-list ----
//
// gsKit_vram_alloc() is a one-way bump allocator (gsGlobal->CurrentPointer);
// there is no per-block free, only gsKit_vram_clear() which wipes ALL user
// VRAM. So glDeleteTextures() / texture re-upload used to leak VRAM forever:
// the bump pointer kept climbing until the 4 MB GS ran out and new textures
// (terrain/items/gui_blocks) silently failed to upload = missing hotbar icons.
//
// Fix: recycle freed VRAM blocks here. ps2_vram_alloc() first tries to reuse a
// freed block of equal-or-greater size (first fit), only bumping CurrentPointer
// via gsKit when nothing fits. Game textures use a small fixed set of sizes that
// repeat across world reloads, so reuse keeps VRAM bounded.
#define PS2_MAX_VRAM_FREE 256
struct PS2VramBlock { u32 vram; u32 size; };
static PS2VramBlock s_vramFree[PS2_MAX_VRAM_FREE];
static int          s_vramFreeCount = 0;

static u32 ps2_vram_alloc(u32 size) {
    // Reuse the smallest freed block that still fits (reduces wasted slack).
    int best = -1;
    for (int i = 0; i < s_vramFreeCount; i++) {
        if (s_vramFree[i].size >= size &&
            (best < 0 || s_vramFree[i].size < s_vramFree[best].size))
            best = i;
    }
    if (best >= 0) {
        u32 vram = s_vramFree[best].vram;
        s_vramFree[best] = s_vramFree[--s_vramFreeCount];
        return vram;
    }
    return gsKit_vram_alloc(gsGlobal, size, GSKIT_ALLOC_USERBUFFER);
}

static void ps2_vram_free(u32 vram, u32 size) {
    if (vram == GSKIT_ALLOC_ERROR || size == 0)
        return;
    if (s_vramFreeCount < PS2_MAX_VRAM_FREE) {
        s_vramFree[s_vramFreeCount].vram = vram;
        s_vramFree[s_vramFreeCount].size = size;
        s_vramFreeCount++;
    }
    // If the free-list is full we simply drop the block (leak it); 256 distinct
    // freed sizes is far more than this game ever churns, so this never trips.
}

extern "C" int ps2_dbg_vram_current_kb()
{
    return gsGlobal ? (int)(gsGlobal->CurrentPointer / 1024) : 0;
}

extern "C" int ps2_dbg_vram_free_blocks()
{
    return s_vramFreeCount;
}

extern "C" int ps2_dbg_vram_recycled_kb()
{
    u32 total = 0;
    for (int i = 0; i < s_vramFreeCount; ++i)
        total += s_vramFree[i].size;
    return (int)(total / 1024);
}

#if PS2_TEX_MIPS
// Drops every level beyond 0: index copies, VRAM blocks and the dirty bits.
// Called on delete, on a level-0 re-specification (the chain belongs to the
// old image) and from the CT16 fallback paths that never had a chain.
static void ps2_texture_mip_release(PS2Tex& te) {
    for (int i = 0; i < PS2_TERRAIN_MIP_MAX_LEVELS; i++) {
        PS2Tex::MipLevel& lv = te.mip[i];
        if (lv.idx) { free(lv.idx); lv.idx = nullptr; }
        ps2_vram_free(lv.vram, lv.vramSize);
        lv.vram = 0;
        lv.vramSize = 0;
        lv.width = lv.height = lv.tbw = 0;
    }
    te.mipLevels = 0;
    te.dirtyMipLevels = 0;
}

static bool ps2_texture_upload_mip_level(PS2Tex& te, int level, int width, int height,
                                         const void* pixels);
static bool ps2_texture_flush_mip_levels(PS2Tex& te);
#endif

bool ps2_texture_valid(unsigned int name) {
    return name > 0 && name < PS2_MAX_TEX &&
           s_tex[name].valid && s_tex[name].uploaded;
}

bool ps2_texture_discard_cpu_mirror(unsigned int name) {
    if (name == 0 || name >= PS2_MAX_TEX)
        return false;
    PS2Tex& te = s_tex[name];
    // If the first upload could not be sent immediately, ps2_texture_resolve()
    // still needs the mirror to retry. Never discard while a deferred upload is
    // pending or before the texture has reached GS VRAM at least once.
    if (!te.valid || !te.uploaded || te.dirtyUpload)
        return false;
#if PS2_TEX_MIPS
    // Same rule for a pending whole-level re-send: it reads the level's index
    // copy and the CLUT, so nothing here can go until resolve() has sent it.
    if (te.dirtyMipLevels != 0)
        return false;
#endif
#ifdef PS2_ENABLE_PSMT8
    if (te.dirtyLegacyClut)
        return false;
#endif

    if (te.cpuMem) {
        free(te.cpuMem);
        te.cpuMem = nullptr;
    }
#ifdef PS2_ENABLE_PSMT8
    if (te.cpuIdx) {
        free(te.cpuIdx);
        te.cpuIdx = nullptr;
    }
    if (te.clut) {
        free(te.clut);
        te.clut = nullptr;
        te.gs.Clut = nullptr;
    }
    if (te.legacyClut) {
        free(te.legacyClut);
        te.legacyClut = nullptr;
    }
    if (te.remap) {
        free(te.remap);
        te.remap = nullptr;
    }
#endif
#if PS2_TEX_MIPS
    for (int i = 0; i < PS2_TERRAIN_MIP_MAX_LEVELS; i++) {
        if (te.mip[i].idx) {
            free(te.mip[i].idx);
            te.mip[i].idx = nullptr;
        }
    }
#endif
    te.gs.Mem = nullptr;
    return true;
}

// Kept as an extern "C" entry point because RenderEngine calls it to retry a
// texture whose load failed (mid-game OOM/VRAM-full) instead of caching a
// permanently broken id -- the "flat white GUI" bug.


void ps2_texture_generate_names(int count, unsigned int* names) {
    if (count <= 0 || names == nullptr)
        return;
    int found = 0;
    for (unsigned int i = 1; i < PS2_MAX_TEX && found < count; i++) {
        if (!s_tex[i].valid) {
            s_tex[i].valid = true; s_tex[i].uploaded = false;
            s_tex[i].tileAtlas = false;
            s_tex[i].cpuMem = nullptr;
            s_tex[i].dirtyUpload = false;
            s_tex[i].dirtyPageRows = 0;
#ifdef PS2_ENABLE_PSMT8
            s_tex[i].cpuIdx = nullptr;
            s_tex[i].clut = nullptr;
            s_tex[i].legacyClut = nullptr;
            s_tex[i].remap = nullptr;
            s_tex[i].normalVramClut = 0;
            s_tex[i].legacyVramClut = 0;
            s_tex[i].clutVramSize = 0;
            s_tex[i].dirtyLegacyClut = false;
#endif
#if PS2_TEX_MIPS
            memset(s_tex[i].mip, 0, sizeof(s_tex[i].mip));
            s_tex[i].mipLevels = 0;
            s_tex[i].dirtyMipLevels = 0;
#endif
            memset(&s_tex[i].gs, 0, sizeof(GSTEXTURE));
            names[found++] = i;
        }
    }
    while (found < count)
        names[found++] = 0;
}
void ps2_texture_bind(unsigned int name) {
    s_boundTex = name;
}

unsigned int ps2_texture_bound_name() {
    return s_boundTex;
}

void ps2_texture_set_tile_atlas(unsigned int name, bool tileAtlas) {
    if (name > 0 && name < PS2_MAX_TEX && s_tex[name].valid)
        s_tex[name].tileAtlas = tileAtlas;
}

bool ps2_texture_bound_is_tile_atlas() {
    return s_boundTex > 0 && s_boundTex < PS2_MAX_TEX &&
           s_tex[s_boundTex].valid && s_tex[s_boundTex].tileAtlas;
}

void ps2_texture_set_legacy_world_pass(bool enabled) {
    s_legacyWorldPass = enabled;
}

void ps2_texture_delete_names(int count, const unsigned int* names) {
    if (count <= 0 || names == nullptr)
        return;
    for (int i = 0; i < count; i++) {
        const unsigned int id = names[i];
        if (id > 0 && id < PS2_MAX_TEX && s_tex[id].valid) {
            if (s_tex[id].cpuMem) { free(s_tex[id].cpuMem); s_tex[id].cpuMem = nullptr; }
#ifdef PS2_ENABLE_PSMT8
            if (s_tex[id].cpuIdx) { free(s_tex[id].cpuIdx); s_tex[id].cpuIdx = nullptr; }
            if (s_tex[id].clut)   { free(s_tex[id].clut);   s_tex[id].clut = nullptr; }
            if (s_tex[id].legacyClut) { free(s_tex[id].legacyClut); s_tex[id].legacyClut = nullptr; }
            if (s_tex[id].remap)  { free(s_tex[id].remap);  s_tex[id].remap = nullptr; }
            ps2_vram_free(s_tex[id].normalVramClut, s_tex[id].clutVramSize);
            ps2_vram_free(s_tex[id].legacyVramClut, s_tex[id].clutVramSize);
            s_tex[id].normalVramClut = 0;
            s_tex[id].legacyVramClut = 0;
            s_tex[id].clutVramSize = 0;
            s_tex[id].gs.VramClut = 0;
            s_tex[id].gs.Clut = nullptr;
            s_tex[id].dirtyLegacyClut = false;
#endif
#if PS2_TEX_MIPS
            ps2_texture_mip_release(s_tex[id]);
#endif
            ps2_vram_free(s_tex[id].gs.Vram, s_tex[id].vramSize);
            s_tex[id].vramSize = 0;
            s_tex[id].gs.Vram = 0;
            s_tex[id].gs.Mem = nullptr;
            s_tex[id].dirtyUpload = false;
            s_tex[id].valid = s_tex[id].uploaded = false;
        }
    }
}

// GS page geometry for the two formats this backend stores textures in.
//
// A page is 8KB in every format; only its texel dimensions change. Both of ours
// are 64 texel rows tall, and that is the granularity a partial upload can
// address -- not the 16x16 tile TextureFX actually rewrites.
//
// The reason is that gsKit_texture_send() hardcodes TRXPOS to (0,0), so the only
// way to aim a transfer somewhere other than the start of the buffer is to move
// the destination base pointer, and that pointer addresses pages. Within a page
// row the buffer is linear, so page row N of a texture that is a whole number of
// pages wide begins at byte offset N * pageH * width * bytesPerTexel -- the same
// offset as in the CPU copy, which is what makes the source pointer arithmetic
// below the plain linear one.
static bool ps2_texture_page_geometry(u32 psm, int* pageW, int* pageH) {
    if (psm == GS_PSM_T8)   { *pageW = 128; *pageH = 64; return true; }
    if (psm == GS_PSM_CT16) { *pageW = 64;  *pageH = 64; return true; }
    return false;
}

// True when this texture can be uploaded one page row at a time. A width that is
// not a whole number of pages leaves TBW padding between page rows, which breaks
// the contiguous-slab assumption the byte offset above depends on.
static bool ps2_texture_supports_partial(const PS2Tex& te) {
#if PS2_TEXTURE_PARTIAL_UPLOAD
    int pageW = 0, pageH = 0;
    if (!ps2_texture_page_geometry(te.gs.PSM, &pageW, &pageH))
        return false;
    if (te.gs.Width == 0 || te.gs.Height == 0)
        return false;
    if ((te.gs.Width % (u32)pageW) != 0 || (te.gs.Height % (u32)pageH) != 0)
        return false;
    return (te.gs.Height / (u32)pageH) <= 32; // one bit per page row
#else
    (void)te;
    return false;
#endif
}

// gsKit_texture_send() memaligns a DMA-chain packet and never checks the
// result — under heap exhaustion it writes the GIF tags through NULL (TLB-miss
// storm at 0x0..0x98). For sub-512KB textures the packet EMBEDS the texture
// data (disasm: alloc = (9 + data_qwc + 2)*16 ≈ texture bytes + tags), so the
// probe must cover the full texture size — a small fixed probe "passed" while
// the real ~64KB terrain re-send still failed. gsKit ships prebuilt, so probe
// first and skip the send (retrying via dirtyUpload) when it cannot fit.
static u32 ps2_upload_packet_bytes_for(int width, int height, int psm) {
    return gsKit_texture_size_ee(width, height, psm) + 1024;
}

static u32 ps2_upload_packet_bytes(const GSTEXTURE* t) {
    return ps2_upload_packet_bytes_for((int)t->Width, (int)t->Height, (int)t->PSM);
}

static bool ps2_heap_can_upload_bytes(size_t need) {
    void* probe = memalign(64, need);
    if (!probe)
        return false;
    free(probe);
    return true;
}

// Parked upload block — the fix for the heap creep that ends in "Out of memory!".
//
// The animated terrain tiles (water/lava/fire/portal) mark terrain.png dirty
// every tick, so the deferred flush below re-sends it -- the whole 256x256 atlas
// before PS2_TEXTURE_PARTIAL_UPLOAD, the dirty page rows now, but a recurring
// transient block either way.
// Each send is a memalign(64, ~65.7KB) inside gsKit that is freed again before
// the call returns, and the probe took a second one. That is
// two ~64KB transient blocks per tick, forever, at the top of the heap: any
// small allocation that outlives one of them (a chunk, a mesh growth, an
// entity) lands in the hole, and the next 64KB request no longer fits, so the
// arena sbrk's another ~68KB. The FRAME log showed exactly that — mallocUsed
// flat while the arena grew in 68KB steps until free hit 0.
//
// So instead of leaving that block transient, hold one permanently and lend it
// to gsKit: free it immediately before the send and re-take it immediately
// after. Nothing else can run in between (single threaded), so the send always
// finds a hole of the right size in the same place, and it can never fail —
// which also keeps the NULL-memalign TLB-miss guard this replaced.
static void*  s_uploadPark      = nullptr;
static size_t s_uploadParkBytes = 0;
// Only park for the recurring in-world sizes. A one-off oversized upload
// (startup art) is better served by the probe path than by pinning RAM.
static const size_t PS2_UPLOAD_PARK_MAX = 160 * 1024;

// Bytes currently lent to gsKit, or 0 when the park is not lent out. Split from
// the upload itself so the whole-texture and per-page-row paths share one park
// instead of each fighting the heap with its own transient block.
static size_t s_uploadParkLent = 0;

// Frees the park so gsKit's own memalign lands in the hole it leaves. Returns
// false when the heap cannot serve `need` at all; the caller must then skip the
// send rather than let gsKit DMA through a NULL packet.
static bool ps2_upload_park_take(size_t need) {
    if (need <= PS2_UPLOAD_PARK_MAX) {
        if (s_uploadPark == nullptr || s_uploadParkBytes < need) {
            // Grow the park to the largest recurring packet. A bigger park than
            // gsKit asks for is fine: malloc splits it for the send and
            // coalesces the pieces back when gsKit frees, so the re-take gets
            // the same block again.
            if (s_uploadPark != nullptr) {
                free(s_uploadPark);
                s_uploadPark = nullptr;
                s_uploadParkBytes = 0;
            }
            void* p = memalign(64, need);
            if (p != nullptr) {
                s_uploadPark = p;
                s_uploadParkBytes = need;
            }
        }
        if (s_uploadPark != nullptr && s_uploadParkBytes >= need) {
            s_uploadParkLent = s_uploadParkBytes;
            free(s_uploadPark);
            s_uploadPark = nullptr;
            return true;
        }
    }

    // No park (heap too tight to hold one, or a one-off oversized texture):
    // fall back to probing before the unchecked memalign inside gsKit.
    s_uploadParkLent = 0;
    return ps2_heap_can_upload_bytes(need);
}

static void ps2_upload_park_return() {
    if (s_uploadParkLent == 0)
        return;
    s_uploadPark = memalign(64, s_uploadParkLent);
    s_uploadParkBytes = (s_uploadPark != nullptr) ? s_uploadParkLent : 0;
    s_uploadParkLent = 0;
}

// Uploads `t`, preferring the parked block. Returns false only when the heap
// cannot serve the packet at all — the caller then keeps dirtyUpload set and
// retries on a later frame instead of letting gsKit DMA through a NULL packet.
static bool ps2_texture_upload_guarded(GSTEXTURE* t) {
    if (!ps2_upload_park_take((size_t)ps2_upload_packet_bytes(t)))
        return false;
    gsKit_texture_upload(gsGlobal, t);
    ps2_upload_park_return();
    return true;
}

#ifdef PS2_ENABLE_PSMT8
static bool ps2_texture_upload_clut(const u16* clut, u32 vramClut) {
    if (clut == nullptr || vramClut == GSKIT_ALLOC_ERROR || vramClut == 0)
        return false;
    const size_t need = static_cast<size_t>(ps2_upload_packet_bytes_for(16, 16, GS_PSM_CT16));
    if (!ps2_upload_park_take(need))
        return false;
    gsKit_texture_send((u32*)clut, 16, 16, vramClut, GS_PSM_CT16, 1, GS_CLUT_PALLETE);
    ps2_upload_park_return();
    return true;
}
#endif

bool ps2_texture_upload_rgba(unsigned int name, int level, int width, int height,
                             const void* pixels) {
    if (!pixels || !gsGlobal) return false;
    const unsigned int id = name;
    if (id == 0 || id >= PS2_MAX_TEX || !s_tex[id].valid) return false;
#if PS2_TEX_MIPS
    if (level != 0)
        return ps2_texture_upload_mip_level(s_tex[id], level, width, height, pixels);
#else
    if (level != 0) return false;
#endif
    const int w = width;
    const int h = height;
    if (w <= 0 || h <= 0) return false;
    if (static_cast<size_t>(w) > std::numeric_limits<size_t>::max() / static_cast<size_t>(h)) return false;
    const size_t pixelCount = static_cast<size_t>(w) * static_cast<size_t>(h);
    if (pixelCount > static_cast<size_t>(std::numeric_limits<u32>::max()) ||
        pixelCount > std::numeric_limits<size_t>::max() / sizeof(u16)) return false;

    PS2Tex& te = s_tex[id];
    if (te.cpuMem) { free(te.cpuMem); te.cpuMem = nullptr; }
    // Re-specifying an existing texture: recycle its old VRAM block first.
#if PS2_TEX_MIPS
    ps2_texture_mip_release(te);
#endif
    ps2_vram_free(te.gs.Vram, te.vramSize);
    te.vramSize = 0;
    te.gs.Vram = 0;
    te.gs.Mem = nullptr;
    te.uploaded = false;
    te.dirtyUpload = false;
    te.dirtyPageRows = 0;
#ifdef PS2_ENABLE_PSMT8
    if (te.cpuIdx) { free(te.cpuIdx); te.cpuIdx = nullptr; }
    if (te.clut)   { free(te.clut);   te.clut = nullptr; }
    if (te.legacyClut) { free(te.legacyClut); te.legacyClut = nullptr; }
    if (te.remap)  { free(te.remap);  te.remap = nullptr; }
    ps2_vram_free(te.normalVramClut, te.clutVramSize);
    ps2_vram_free(te.legacyVramClut, te.clutVramSize);
    te.normalVramClut = 0;
    te.legacyVramClut = 0;
    te.clutVramSize = 0;
    te.gs.VramClut = 0;
    te.gs.Clut = nullptr;
    te.dirtyLegacyClut = false;
#endif

    te.gs.Width  = (u32)w;
    te.gs.Height = (u32)h;
    te.gs.Filter = GS_FILTER_NEAREST;
    te.gs.Delayed = 0;

    const u32 npx = static_cast<u32>(pixelCount);
    // CT16 conversion buffer: pixel source for the T8 palettizer, or the
    // long-lived CPU copy on the CT16 path. 64-byte aligned for GS DMA.
    u16* px16 = (u16*)memalign(64, npx * sizeof(u16));
    if (!px16) {
        MC_LOG_ERROR("render", "[PS2] ps2_texture_upload_rgba: OUT OF RAM for %dx%d px16 (tex id=%u)\n", w, h, id);
        te.valid = false;
        return false;
    }
    const u8* src = (const u8*)pixels;
    if (te.tileAtlas) {
        // Cutout atlases are alpha-tested, so a transparent texel's RGB never
        // reaches the framebuffer -- but it does count as a palette colour.
        // RenderEngine recolours those texels per tile when it builds a mip
        // chain (computeTileAverageOpaqueColors), which would add up to one
        // entry per tile and push terrain.png past 256. Fold them all into one.
        for (u32 p = 0; p < npx; p++)
            px16[p] = src[p * 4 + 3] >= 128 ? rgba_to_psmct16(src + p * 4) : (u16)0;
    } else {
        for (u32 p = 0; p < npx; p++)
            px16[p] = rgba_to_psmct16(src + p * 4);
    }

#ifdef PS2_ENABLE_PSMT8
    // 8-bit palettized: 1 byte/pixel + 512-byte CLUT. Exact for <=256 unique
    // CT16 colors, nearest-match quantized beyond that (see ps2_palettize_ct16).
    te.cpuIdx = (u8*)memalign(64, npx);
    te.clut   = (u16*)memalign(64, 256 * sizeof(u16));
    te.legacyClut = (u16*)memalign(64, 256 * sizeof(u16));
    u16 paletteLin[256];
    int nPal = 0;
    if (te.cpuIdx && te.clut && te.legacyClut)
        nPal = ps2_palettize_ct16(px16, npx, te.cpuIdx, paletteLin);
    free(px16);
    if (nPal <= 0) {
        MC_LOG_ERROR("render", "[PS2] ps2_texture_upload_rgba: palettize failed for %dx%d tex id=%u\n", w, h, id);
        if (te.cpuIdx) { free(te.cpuIdx); te.cpuIdx = nullptr; }
        if (te.clut)   { free(te.clut);   te.clut = nullptr; }
        if (te.legacyClut) { free(te.legacyClut); te.legacyClut = nullptr; }
        te.valid = false;
        return false;
    }
    // CSM1 wants index bits 3/4 swapped in storage; unused entries stay
    // transparent black.
    memset(te.clut, 0, 256 * sizeof(u16));
    for (int i = 0; i < nPal; i++)
        te.clut[ps2_clut_csm1_pos(i)] = paletteLin[i];
    ps2_build_legacy_clut(te.clut, te.legacyClut);

    te.gs.PSM             = GS_PSM_T8;
    te.gs.ClutPSM         = GS_PSM_CT16;
    te.gs.Clut            = (u32*)te.clut;
    te.gs.ClutStorageMode = GS_CLUT_STORAGE_CSM1;
    gsKit_setup_tbw(&te.gs);

    u32 vsz  = gsKit_texture_size(w, h, GS_PSM_T8);
    u32 cvsz = gsKit_texture_size(16, 16, GS_PSM_CT16);
    te.gs.Vram = ps2_vram_alloc(vsz);
    te.normalVramClut = ps2_vram_alloc(cvsz);
    te.legacyVramClut = ps2_vram_alloc(cvsz);
    te.gs.VramClut = te.normalVramClut;
    if (te.gs.Vram == GSKIT_ALLOC_ERROR || te.normalVramClut == GSKIT_ALLOC_ERROR ||
        te.legacyVramClut == GSKIT_ALLOC_ERROR) {
        MC_LOG_WARN("render", "[PS2] ps2_texture_upload_rgba: VRAM full for %dx%d tex id=%u\n", w, h, id);
        if (te.gs.Vram != GSKIT_ALLOC_ERROR) ps2_vram_free(te.gs.Vram, vsz);
        if (te.normalVramClut != GSKIT_ALLOC_ERROR) ps2_vram_free(te.normalVramClut, cvsz);
        if (te.legacyVramClut != GSKIT_ALLOC_ERROR) ps2_vram_free(te.legacyVramClut, cvsz);
        te.gs.Vram = 0; te.gs.VramClut = 0;
        te.normalVramClut = 0; te.legacyVramClut = 0;
        te.vramSize = te.clutVramSize = 0;
        free(te.cpuIdx); te.cpuIdx = nullptr;
        free(te.clut);   te.clut = nullptr;
        free(te.legacyClut); te.legacyClut = nullptr;
        te.valid = false;
        return false;
    }
    te.vramSize = vsz;
    te.clutVramSize = cvsz;

    te.gs.Mem = (u32*)te.cpuIdx;
    if (!ps2_texture_upload_guarded(&te.gs)) // sends indices + CLUT
        te.dirtyUpload = true; // OOM right now: retry from the deferred flush
    te.gs.Mem = nullptr;
    te.dirtyLegacyClut = !ps2_texture_upload_clut(te.legacyClut, te.legacyVramClut);
#else
    // PSM_CT16 (A1B5G5R5) uses 2 bytes/pixel, halving VRAM vs CT32.
    // Alpha is 1-bit: fully transparent (A=0) vs opaque (A=1), sufficient
    // for terrain cutouts. GUI translucency is approximated via GS alpha blend.
    te.gs.PSM    = GS_PSM_CT16;
    te.gs.ClutStorageMode = GS_CLUT_NONE;
    te.gs.Clut = nullptr; te.gs.VramClut = 0;
    te.gs.ClutPSM = 0;

    gsKit_setup_tbw(&te.gs);

    u32 vsz = gsKit_texture_size(w, h, GS_PSM_CT16);
    te.gs.Vram = ps2_vram_alloc(vsz);
    if (te.gs.Vram == GSKIT_ALLOC_ERROR) {
        MC_LOG_WARN("render", "[PS2] ps2_texture_upload_rgba: VRAM full for %dx%d tex id=%u\n", w, h, id);
        te.vramSize = 0;
        te.valid = false;
        free(px16);
        return false;
    }
    te.vramSize = vsz;
    te.cpuMem = px16;

    te.gs.Mem = (u32*)te.cpuMem;
    if (!ps2_texture_upload_guarded(&te.gs))
        te.dirtyUpload = true; // OOM right now: retry from the deferred flush
    te.gs.Mem = nullptr;
#endif
    te.uploaded = true;
    return true;
}

#ifdef PS2_ENABLE_PSMT8
// Map one CT16 color to the texture's palette index. Lazy 64K-entry memo
// (built only for textures that actually receive glTexSubImage2D traffic —
// in practice just terrain.png's animated water/lava/fire tiles, whose
// colors come from the original texture and therefore hit the palette).
static u8 ps2_t8_remap_color(PS2Tex& te, u16 v) {
    if (!te.remap) {
        te.remap = (u16*)malloc(65536 * sizeof(u16));
        if (!te.remap) return 0;
        memset(te.remap, 0xFF, 65536 * sizeof(u16));
        for (int i = 0; i < 256; i++) {
            u16 c = te.clut[ps2_clut_csm1_pos(i)];
            if (te.remap[c] == 0xFFFF)
                te.remap[c] = (u16)i;
        }
    }
    u16 m = te.remap[v];
    if (m != 0xFFFF) return (u8)m;
    int best = 0, bestD = 0x7FFFFFFF;
    for (int i = 0; i < 256; i++) {
        int d = ps2_ct16_dist(v, te.clut[ps2_clut_csm1_pos(i)]);
        if (d < bestD) { bestD = d; best = i; }
    }
    te.remap[v] = (u16)best;
    return (u8)best;
}
#endif

#if PS2_TEXTURE_PARTIAL_UPLOAD
// Sends texel rows [y0, y0 + rows) of `te` to GS VRAM. Both bounds are page-row
// aligned, so the source rows are contiguous in the CPU copy and the destination
// is a contiguous run of GS pages the base pointer can be aimed at directly.
//
// This is gsKit_texture_upload() with band dimensions substituted for the
// texture's own, CLUT handling included. The palette send is not redundant on
// the T8 path: it is the call that carries the TEXFLUSH, and without it the
// rasteriser can keep sampling texels the GS still holds from before the
// transfer.
static bool ps2_texture_send_band(PS2Tex& te, int y0, int rows) {
    const int width = (int)te.gs.Width;

#ifdef PS2_ENABLE_PSMT8
    const u8* src = (const u8*)te.cpuIdx;
    const u32 byteOffset = (u32)y0 * (u32)width;          // PSMT8: 1 byte/texel
#else
    const u8* src = (const u8*)te.cpuMem;
    const u32 byteOffset = (u32)y0 * (u32)width * 2u;     // PSMCT16: 2 bytes/texel
#endif
    if (src == nullptr)
        return false;

    const size_t need = (size_t)ps2_upload_packet_bytes_for(width, rows, (int)te.gs.PSM);
    if (!ps2_upload_park_take(need))
        return false;

    gsKit_texture_send((u32*)(src + byteOffset), width, rows,
                       te.gs.Vram + byteOffset, te.gs.PSM, te.gs.TBW,
                       GS_CLUT_TEXTURE);
#ifdef PS2_ENABLE_PSMT8
    gsKit_texture_send((u32*)te.clut, 16, 16, te.normalVramClut,
                       te.gs.ClutPSM, 1, GS_CLUT_PALLETE);
#endif
    ps2_upload_park_return();
    return true;
}
#endif // PS2_TEXTURE_PARTIAL_UPLOAD

#if PS2_TEX_MIPS
// Sends every level whose dirty bit is set, whole, followed by the CLUT. The
// palette is what carries the TEXFLUSH (see ps2_texture_send_band), and mip
// levels are the only traffic that can change without level 0 changing with
// it -- a chain uploaded after level 0 at load, or a level-only retry. Each
// level is at most a quarter of the atlas, so the whole chain is under the
// parked packet size. Returns false and leaves the bits set when the heap
// cannot serve a packet; ps2_texture_resolve() retries on a later bind.
static bool ps2_texture_flush_mip_levels(PS2Tex& te) {
    if (te.dirtyMipLevels == 0)
        return true;
    if (te.clut == nullptr)
        return false;
    bool sentAny = false;
    for (int i = 0; i < te.mipLevels; i++) {
        const u32 bit = 1u << i;
        if ((te.dirtyMipLevels & bit) == 0)
            continue;
        PS2Tex::MipLevel& lv = te.mip[i];
        if (lv.idx == nullptr)
            return false;
        const size_t need = (size_t)ps2_upload_packet_bytes_for((int)lv.width, (int)lv.height, GS_PSM_T8);
        if (!ps2_upload_park_take(need))
            return false;
        gsKit_texture_send((u32*)lv.idx, (int)lv.width, (int)lv.height, lv.vram,
                           GS_PSM_T8, lv.tbw, GS_CLUT_TEXTURE);
        ps2_upload_park_return();
        te.dirtyMipLevels &= ~bit;
        sentAny = true;
    }
    if (sentAny) {
        const size_t need = (size_t)ps2_upload_packet_bytes_for(16, 16, GS_PSM_CT16);
        if (ps2_upload_park_take(need)) {
            gsKit_texture_send((u32*)te.clut, 16, 16, te.normalVramClut,
                               te.gs.ClutPSM, 1, GS_CLUT_PALLETE);
            ps2_upload_park_return();
        }
    }
    return te.dirtyMipLevels == 0;
}

// Specifies level `level` (1..PS2_TERRAIN_MIP_MAX_LEVELS) of a tile atlas from
// RGBA8, remapped through the level-0 palette. Levels must arrive in order --
// RenderEngine::setupTexture uploads them that way -- and match the level-0
// dimensions shifted by the level; anything else is silently declined and the
// chain stays at whatever depth it reached, which TEX1.MXL reports.
static bool ps2_texture_upload_mip_level(PS2Tex& te, int level, int width, int height,
                                         const void* pixels) {
    if (!te.valid || !te.uploaded || !te.tileAtlas)
        return false;
    if (te.gs.PSM != GS_PSM_T8 || te.clut == nullptr)
        return false;
    if (level < 1 || level > PS2_TERRAIN_MIP_MAX_LEVELS || level != te.mipLevels + 1)
        return false;
    if (width <= 0 || height <= 0 ||
        (u32)width != (te.gs.Width >> level) || (u32)height != (te.gs.Height >> level))
        return false;

    PS2Tex::MipLevel& lv = te.mip[level - 1];
    const u32 npx = (u32)width * (u32)height;
    lv.idx = (u8*)memalign(64, npx);
    if (lv.idx == nullptr)
        return false;
    const u8* src = (const u8*)pixels;
    for (u32 p = 0; p < npx; p++) {
        const u16 c = src[p * 4 + 3] >= 128 ? rgba_to_psmct16(src + p * 4) : (u16)0;
        lv.idx[p] = ps2_t8_remap_color(te, c);
    }

    // gsKit's own width rounding for the buffer width: a T8 level narrower than
    // a page still addresses a full 128-texel page row, and the VRAM size it
    // computes covers that padding.
    GSTEXTURE probe;
    memset(&probe, 0, sizeof(probe));
    probe.Width = (u32)width;
    probe.Height = (u32)height;
    probe.PSM = GS_PSM_T8;
    gsKit_setup_tbw(&probe);
    lv.tbw = probe.TBW;
    lv.vramSize = gsKit_texture_size(width, height, GS_PSM_T8);
    lv.vram = ps2_vram_alloc(lv.vramSize);
    if (lv.vram == GSKIT_ALLOC_ERROR) {
        MC_LOG_WARN("render", "[PS2] ps2_texture_upload_rgba: VRAM full for mip level %d (%dx%d) tex id=%u\n",
                    level, width, height, (unsigned int)(&te - s_tex));
        free(lv.idx);
        lv.idx = nullptr;
        lv.vram = 0;
        lv.vramSize = 0;
        return false;
    }
    lv.width = (u32)width;
    lv.height = (u32)height;
    te.mipLevels = level;
    te.dirtyMipLevels |= 1u << (level - 1);
    ps2_texture_flush_mip_levels(te); // heap too tight: the bit stays for resolve()
    return true;
}
#endif // PS2_TEX_MIPS

// Filter requests are honored for standalone textures so menu backgrounds and
// other intentionally blurred images can use the GS bilinear sampler. Tile
// atlases stay nearest-filtered because sampling across a tile boundary causes
// visible bleeding. Wrap is still selected per primitive by ps2_select_clamp.
// The mipmap request is not recorded either: the level count a texture actually
// holds (ps2_texture_upload_rgba with level > 0) is what TEX1 reports.
void ps2_texture_set_parameters(bool linear, bool mipmaps, bool clamp) {
    (void)mipmaps;
    (void)clamp;

    if (s_boundTex == 0 || s_boundTex >= PS2_MAX_TEX)
        return;

    PS2Tex& te = s_tex[s_boundTex];
    if (!te.valid)
        return;

    if (te.tileAtlas)
        linear = false;

    const int requestedFilter = linear ? GS_FILTER_LINEAR : GS_FILTER_NEAREST;
    if (te.gs.Filter == requestedFilter)
        return;

    // The deferred 2D batch keeps a GSTEXTURE pointer and reads Filter when it
    // is emitted, so flush geometry created under the old sampler first.
    ps2_draw_2d_flush_pending();
    te.gs.Filter = requestedFilter;
}

#if PS2_TEX_MIPS
// Patches a rectangle of one mip level's index copy. Like the level-0 path
// this only marks the level; ps2_texture_resolve() sends it whole (16 KB at
// most) ahead of the level-0 bands, so the animated tiles keep their chain in
// step without a transfer per touched tile per level.
static bool ps2_texture_upload_sub_mip_level(PS2Tex& te, int level, int x, int y,
                                             int width, int height, const void* pixels) {
    if (level < 1 || level > te.mipLevels)
        return false;
    PS2Tex::MipLevel& lv = te.mip[level - 1];
    if (lv.idx == nullptr || te.clut == nullptr)
        return false;
    if (x < 0 || y < 0 || width <= 0 || height <= 0)
        return false;
    const std::int64_t endX = static_cast<std::int64_t>(x) + static_cast<std::int64_t>(width);
    const std::int64_t endY = static_cast<std::int64_t>(y) + static_cast<std::int64_t>(height);
    if (endX > static_cast<std::int64_t>(lv.width) || endY > static_cast<std::int64_t>(lv.height))
        return false;
    const u8* src = (const u8*)pixels;
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            const int dp = (y + row) * (int)lv.width + (x + col);
            const int sp = row * width + col;
            const u16 c = src[sp * 4 + 3] >= 128 ? rgba_to_psmct16(src + sp * 4) : (u16)0;
            lv.idx[dp] = ps2_t8_remap_color(te, c);
        }
    }
    te.dirtyMipLevels |= 1u << (level - 1);
    return true;
}
#endif // PS2_TEX_MIPS

bool ps2_texture_upload_sub_rgba(unsigned int name, int level, int x, int y,
                                 int width, int height, const void* pixels) {
    if (!pixels) return false;
    const unsigned int id = name;
    if (id == 0 || id >= PS2_MAX_TEX || !s_tex[id].valid) return false;
#if PS2_TEX_MIPS
    if (level != 0)
        return ps2_texture_upload_sub_mip_level(s_tex[id], level, x, y, width, height, pixels);
#else
    if (level != 0) return false;
#endif
    const int xoff = x;
    const int yoff = y;
    const int w = width;
    const int h = height;
    PS2Tex& te = s_tex[id];
    if (xoff < 0 || yoff < 0 || w <= 0 || h <= 0) return false;
    const std::int64_t endX = static_cast<std::int64_t>(xoff) + static_cast<std::int64_t>(w);
    const std::int64_t endY = static_cast<std::int64_t>(yoff) + static_cast<std::int64_t>(h);
    if (endX > static_cast<std::int64_t>(te.gs.Width) ||
        endY > static_cast<std::int64_t>(te.gs.Height)) return false;
    const u8* src = (const u8*)pixels;
#ifdef PS2_ENABLE_PSMT8
    if (!te.cpuIdx || !te.clut) return false;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int dp = (yoff+row)*(int)te.gs.Width + (xoff+col);
            int sp = row*w + col;
            te.cpuIdx[dp] = ps2_t8_remap_color(te, rgba_to_psmct16(src + sp * 4));
        }
    }
#else
    if (!te.cpuMem) return false;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int dp = (yoff+row)*(int)te.gs.Width + (xoff+col);
            int sp = row*w + col;
            te.cpuMem[dp] = rgba_to_psmct16(src + sp * 4);
        }
    }
#endif
    // Don't re-send per call: terrain.png gets several animated-tile updates per
    // frame (water/lava/fire), and each upload here used to DMA the full texture
    // again. Mark the GS page rows this rectangle covers; the next draw that
    // binds the texture sends those, once.
    te.dirtyUpload = true;
#if PS2_TEXTURE_PARTIAL_UPLOAD
    if (ps2_texture_supports_partial(te)) {
        int pageW = 0, pageH = 0;
        ps2_texture_page_geometry(te.gs.PSM, &pageW, &pageH);
        const int firstRow = yoff / pageH;
        const int lastRow = (yoff + h - 1) / pageH;
        for (int row = firstRow; row <= lastRow; row++)
            te.dirtyPageRows |= (1u << row);
    } else {
        te.dirtyPageRows = 0; // 0 while dirtyUpload is set means "all of it"
    }
#endif
    return true;
}

// Deferred flush point for the sub-image traffic above: animated tiles mark
// themselves dirty and one full send happens here instead of one per touched
// tile. It goes through the parked block so this per-frame ~64KB packet cannot
// walk the top of the heap; on a frame the heap cannot serve it the send is
// skipped and retried next frame (gsKit would otherwise build the packet through
// a NULL memalign result and TLB-miss).
GSTEXTURE* ps2_texture_resolve(unsigned int name) {
    if (name == 0 || name >= PS2_MAX_TEX)
        return nullptr;
    PS2Tex& te = s_tex[name];
    if (!te.valid || !te.uploaded)
        return nullptr;
#if PS2_TEX_MIPS
    // Levels first: the level-0 band or whole-texture send below ends with the
    // normal CLUT and its TEXFLUSH, which then covers the chain as well.
    if (te.dirtyMipLevels != 0)
        ps2_texture_flush_mip_levels(te);
#endif
    if (te.dirtyUpload) {
        bool handledPartial = false;
#if PS2_TEXTURE_PARTIAL_UPLOAD
        if (te.dirtyPageRows != 0 && ps2_texture_supports_partial(te)) {
            handledPartial = true;
            int pageW = 0, pageH = 0;
            ps2_texture_page_geometry(te.gs.PSM, &pageW, &pageH);
            const int pageRowCount = (int)(te.gs.Height / (u32)pageH);
            // Adjacent dirty page rows leave as one band, so the common case of
            // two neighbouring animated tiles still costs a single transfer.
            int row = 0;
            while (row < pageRowCount) {
                if ((te.dirtyPageRows & (1u << row)) == 0) {
                    row++;
                    continue;
                }
                int end = row;
                while (end + 1 < pageRowCount && (te.dirtyPageRows & (1u << (end + 1))) != 0)
                    end++;
                const int span = end - row + 1;
                if (!ps2_texture_send_band(te, row * pageH, span * pageH))
                    break; // heap too tight this frame; the bits stay set
                te.dirtyPageRows &= ~(((1u << span) - 1u) << row);
                row = end + 1;
            }
            if (te.dirtyPageRows == 0)
                te.dirtyUpload = false;
        }
#endif
        if (te.dirtyUpload && !handledPartial) {
#ifdef PS2_ENABLE_PSMT8
            te.gs.VramClut = te.normalVramClut;
            te.gs.Mem = (u32*)te.cpuIdx;
#else
            te.gs.Mem = (u32*)te.cpuMem;
#endif
            if (ps2_texture_upload_guarded(&te.gs)) {
                te.dirtyUpload = false;
                te.dirtyPageRows = 0;
            }
            te.gs.Mem = nullptr;
        }
    }
#ifdef PS2_ENABLE_PSMT8
    if (te.dirtyLegacyClut && te.legacyClut != nullptr)
        te.dirtyLegacyClut = !ps2_texture_upload_clut(te.legacyClut, te.legacyVramClut);

    te.gs.VramClut = s_legacyWorldPass && !te.dirtyLegacyClut
        ? te.legacyVramClut
        : te.normalVramClut;
#endif
    return &te.gs;
}

Ps2TextureSampler ps2_texture_sampler_registers(const GSTEXTURE* texture) {
    Ps2TextureSampler out;
    out.mipmapped = false;
    out.miptbp1 = 0;
    // What every gsKit primitive writes: fixed LOD 0, no chain.
    const int filter = texture ? (int)texture->Filter : GS_FILTER_NEAREST;
    out.tex1 = GS_SETREG_TEX1(1, 0, filter, filter, 0, 0, 0);
#if PS2_TEX_MIPS
    if (texture < &s_tex[0].gs || texture > &s_tex[PS2_MAX_TEX - 1].gs)
        return out;
    const PS2Tex& te = *reinterpret_cast<const PS2Tex*>(texture);
    if (!te.valid || !te.uploaded || te.mipLevels <= 0)
        return out;

    // MMIN 2/3 = NEAREST_MIPMAP_{NEAREST,LINEAR}, 4/5 the LINEAR_MIPMAP pair.
    const int mmin = (filter == GS_FILTER_LINEAR ? 4 : 2) + (PS2_TERRAIN_MIP_LINEAR ? 1 : 0);
    // K is a signed 12-bit field; a negative int would sign-extend across the
    // register through the macro's u64 cast.
    const u64 k = (u64)((unsigned int)PS2_TERRAIN_MIP_LOD_BIAS_K & 0xFFFu);
    out.tex1 = GS_SETREG_TEX1(0, te.mipLevels, filter, mmin, 0, PS2_TERRAIN_MIP_LOD_L, k);
    u32 tbp[3] = { 0, 0, 0 };
    u32 tbw[3] = { 0, 0, 0 };
    for (int l = 0; l < te.mipLevels && l < 3; l++) {
        tbp[l] = te.mip[l].vram / 256;
        tbw[l] = te.mip[l].tbw;
    }
    out.miptbp1 = GS_SETREG_MIPTBP1(tbp[0], tbw[0], tbp[1], tbw[1], tbp[2], tbw[2]);
    out.mipmapped = true;
#endif
    return out;
}

// System-RAM cost of the texture cache, split by what is holding it.
//
// Dynamic textures keep a CPU-side copy so TextureFX/custom animation sub-image
// updates can patch and re-send them. Static named textures release that mirror
// after a successful GS upload through TextureResidencyPolicy. Any mirror still
// present here is therefore dynamic or waiting for a deferred upload retry.
//
//   pix   = the pixel copy itself (cpuIdx on T8, cpuMem on CT16), now normally
//           limited to animated/dynamic textures and deferred upload retries.
//   clut  = normal + Legacy 512-byte CLUT copies while their CPU mirrors live.
//   remap = 128 KB EACH (65536 u16). ps2_t8_remap_color allocates a full
//           CT16-value -> palette-index memo the first time glTexSubImage2D
//           touches a T8 texture, i.e. once per ANIMATED atlas (terrain, items).
//           It is a pure speed memo for a function that can also compute the
//           answer directly, so it is the one line here that can be reclaimed
//           outright rather than traded against image quality.
extern "C" void ps2_dbg_texture_ram_bytes(long* pixOut, long* clutOut, long* remapOut) {
    long pix = 0, clut = 0, remap = 0;
    for (int i = 0; i < PS2_MAX_TEX; i++) {
        const PS2Tex& t = s_tex[i];
        if (!t.valid)
            continue;
        const long npx = (long)t.gs.Width * (long)t.gs.Height;
#ifdef PS2_ENABLE_PSMT8
        if (t.cpuIdx) pix   += npx;               // 1 byte/px
        if (t.clut)   clut  += 256 * (long)sizeof(u16);
        if (t.legacyClut) clut += 256 * (long)sizeof(u16);
        if (t.remap)  remap += 65536 * (long)sizeof(u16);
#endif
#if PS2_TEX_MIPS
        for (int l = 0; l < t.mipLevels; l++) {
            if (t.mip[l].idx) pix += (long)t.mip[l].width * (long)t.mip[l].height;
        }
#endif
        if (t.cpuMem) pix += npx * (long)sizeof(u16); // 2 bytes/px
    }
    if (pixOut)   *pixOut   = pix;
    if (clutOut)  *clutOut  = clut;
    if (remapOut) *remapOut = remap;
}

#endif // PS2_PLATFORM
