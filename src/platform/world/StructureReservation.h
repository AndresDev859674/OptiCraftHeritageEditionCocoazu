#pragma once

#include <vector>

#include "java/Arithmetic.h"
#include "net/minecraft/src/StructureBoundingBox.h"

namespace StructureReservation
{
    inline StructureBoundingBox makeBounds(const StructureBoundingBox &source,
                                           const StructureBoundingBox &area,
                                           bool projectToAreaHeight,
                                           int_t horizontalPadding)
    {
        return StructureBoundingBox(
            JavaArithmetic::intSub(source.minX, horizontalPadding),
            projectToAreaHeight ? area.minY : source.minY,
            JavaArithmetic::intSub(source.minZ, horizontalPadding),
            JavaArithmetic::intAdd(source.maxX, horizontalPadding),
            projectToAreaHeight ? area.maxY : source.maxY,
            JavaArithmetic::intAdd(source.maxZ, horizontalPadding));
    }

    inline bool contains(const std::vector<StructureBoundingBox> &bounds,
                         int_t x, int_t y, int_t z)
    {
        for (const StructureBoundingBox &bound : bounds)
        {
            if (bound.isVecInside(x, y, z))
                return true;
        }
        return false;
    }
}
