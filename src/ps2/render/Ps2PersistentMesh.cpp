#ifdef PS2_PLATFORM

#include "ps2/render/Ps2PersistentMesh.h"

#include "ps2/render/Ps2GeometryCache.h"
#include "ps2/render/Ps2LitCapture.h"

namespace
{
// There are 182 addBox calls across the 70 model classes, and a handle is taken
// per ModelRenderer part rather than per box or per entity, so this ceiling is
// never reached by model geometry alone. Exhausting it returns handle 0, which
// ModelRenderer already reads as "stay on the immediate path".
constexpr int kMaxHandles = 256;

struct Entry
{
    bool used = false;
    int cacheHandle = -1;
};

Entry s_entries[kMaxHandles];

Entry* entryFor(int handle)
{
    if (handle <= 0 || handle > kMaxHandles)
        return nullptr;
    Entry& entry = s_entries[handle - 1];
    return entry.used ? &entry : nullptr;
}
} // namespace

int ps2_persistent_mesh_create()
{
    for (int slot = 0; slot < kMaxHandles; ++slot)
    {
        Entry& entry = s_entries[slot];
        if (entry.used)
            continue;
        entry.used = true;
        entry.cacheHandle = -1;
        return slot + 1; // handle 0 is the caller's failure value
    }
    return 0;
}

void ps2_persistent_mesh_destroy(int handle)
{
    Entry* entry = entryFor(handle);
    if (entry == nullptr)
        return;

    ps2ModelGeometryCache().release(entry->cacheHandle);
    entry->cacheHandle = -1;
    entry->used = false;
}

bool ps2_persistent_mesh_compile(int handle, const RenderInterleavedMesh& mesh)
{
    Entry* entry = entryFor(handle);
    if (entry == nullptr)
        return false;

    // Capture through the lit path explicitly instead of renderCaptureInterleaved.
    // That function falls back to the compact terrain layout when a mesh carries
    // no normals, and the compact layout both drops normals and stores a frozen
    // color -- which would replace the live per-entity tint and the animated
    // lighting with whatever the first frame happened to hold. A model whose
    // boxes emit no normals fails here and stays on the immediate path, which is
    // the current behavior.
    RenderCapturedMesh captured;
    if (!ps2CaptureLitInterleaved(mesh, captured, false))
        return false;

    // Recompiling an already populated handle must not leak its previous slot.
    ps2ModelGeometryCache().release(entry->cacheHandle);
    entry->cacheHandle = ps2ModelGeometryCache().retain(captured);
    return entry->cacheHandle >= 0;
}

bool ps2_persistent_mesh_draw(int handle)
{
    const Entry* entry = entryFor(handle);
    if (entry == nullptr)
        return false;

    const RenderCapturedMesh* mesh = ps2ModelGeometryCache().get(entry->cacheHandle);
    return mesh != nullptr && renderDrawCaptured(*mesh);
}

#endif // PS2_PLATFORM
