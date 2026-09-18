#pragma once

#include "platform/RenderAPI.h"
#include "platform/ExtendedProfiler.h"

#include <utility>

class Ps2ModelGeometryCache
{
public:
    static constexpr int MaxEntries = 128;
    static constexpr std::size_t MaxBytes = 128 * 1024;
    static constexpr std::size_t MaxModelBytes = 32 * 1024;
    static constexpr std::size_t MaxPartBytes = 16 * 1024;

    int retain(RenderCapturedMesh& mesh)
    {
        if (mesh.empty() || mesh.byteSize() > MaxPartBytes)
            return -1;
        int freeSlot = -1;
        for (int i = 0; i < MaxEntries; ++i)
        {
            Entry& entry = entries[i];
            if (entry.references == 0)
            {
                if (freeSlot < 0)
                    freeSlot = i;
            }
            else if (equal(entry.mesh, mesh))
            {
                ++entry.references;
#if MC_LOG_LEVEL > 2
                platformProfileModelCacheRetain(true);
#endif
                return i;
            }
        }
        const std::size_t allocation = mesh.raw.capacity() * sizeof(std::int32_t);
        if (freeSlot < 0 || allocation > MaxBytes - retainedBytes)
            return -1;
        Entry& entry = entries[freeSlot];
        entry.mesh = std::move(mesh);
        entry.references = 1;
        retainedBytes += allocation;
        ++retainedEntries;
#if MC_LOG_LEVEL > 2
        platformProfileModelCacheRetain(false);
#endif
        return freeSlot;
    }

    void release(int handle)
    {
        if (handle < 0 || handle >= MaxEntries || entries[handle].references == 0)
            return;
        Entry& entry = entries[handle];
        if (--entry.references != 0)
            return;
        retainedBytes -= entry.mesh.raw.capacity() * sizeof(std::int32_t);
        RenderCapturedMesh empty;
        entry.mesh = std::move(empty);
        --retainedEntries;
    }

    const RenderCapturedMesh* get(int handle) const
    {
        const RenderCapturedMesh* result = handle >= 0 && handle < MaxEntries && entries[handle].references != 0
            ? &entries[handle].mesh : nullptr;
#if MC_LOG_LEVEL > 2
        platformProfileModelCacheGet(result != nullptr);
#endif
        return result;
    }

    std::size_t bytes() const { return retainedBytes; }
    int size() const { return retainedEntries; }

private:
    static bool equal(const RenderCapturedMesh& a, const RenderCapturedMesh& b)
    {
        return a.vertexCount == b.vertexCount && a.stride == b.stride &&
            a.primitive == b.primitive && a.positionShort == b.positionShort &&
            a.hasTexture == b.hasTexture && a.texCoordOffset == b.texCoordOffset &&
            a.hasColor == b.hasColor && a.colorOffset == b.colorOffset &&
            a.hasNormals == b.hasNormals && a.normalOffset == b.normalOffset &&
            a.hasBrightness == b.hasBrightness && a.brightnessOffset == b.brightnessOffset &&
            a.raw == b.raw;
    }

    struct Entry
    {
        RenderCapturedMesh mesh;
        unsigned int references = 0;
    };

    Entry entries[MaxEntries];
    std::size_t retainedBytes = 0;
    int retainedEntries = 0;
};

inline Ps2ModelGeometryCache& ps2ModelGeometryCache()
{
    static Ps2ModelGeometryCache cache;
    return cache;
}

struct Ps2GeometryCacheStats
{
    std::size_t modelBytes = 0;
    std::size_t inventoryBytes = 0;
    int modelEntries = 0;
    int inventoryEntries = 0;
};

inline Ps2GeometryCacheStats& ps2InventoryGeometryAccounting()
{
    static Ps2GeometryCacheStats stats;
    return stats;
}

inline Ps2GeometryCacheStats ps2GetGeometryCacheStats()
{
    Ps2GeometryCacheStats stats = ps2InventoryGeometryAccounting();
    stats.modelBytes = ps2ModelGeometryCache().bytes();
    stats.modelEntries = ps2ModelGeometryCache().size();
    return stats;
}
