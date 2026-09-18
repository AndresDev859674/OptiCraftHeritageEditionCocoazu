#pragma once

#include <array>
#include <cstddef>

#include "java/Type.h"

class World;

// Collects the render-dirty marks a lighting drain would otherwise issue one
// cell at a time.
//
// Every light value that changes calls World::func_48464_p, which walks up to
// eight WorldRenderers for that single block. A drain touches hundreds of cells
// per frame, nearly all inside the same few chunk sections. While a drain is
// active the marks are accumulated here as one bounding box per section and
// issued once at the end through World::markBlocksDirty, which expands the box
// by one block exactly as the per-cell call did, so neighbouring sections still
// get marked when a changed cell sits on a border.
class LightingDirtyRegions
{
public:
    static constexpr std::size_t SLOT_COUNT = 64;

    bool isActive() const { return active; }
    void begin();
    // Records a changed cell. Flushes an unrelated slot on collision so no mark
    // is ever lost.
    void add(World *world, int_t x, int_t y, int_t z);
    void end(World *world);

private:
    struct Region
    {
        bool used = false;
        int_t sectionX = 0;
        int_t sectionY = 0;
        int_t sectionZ = 0;
        int_t minX = 0, minY = 0, minZ = 0;
        int_t maxX = 0, maxY = 0, maxZ = 0;
    };

    static std::size_t slotOf(int_t sectionX, int_t sectionY, int_t sectionZ);
    static void flush(World *world, Region &region);

    bool active = false;
    std::array<Region, SLOT_COUNT> regions{};
};
