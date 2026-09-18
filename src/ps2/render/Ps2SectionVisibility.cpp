#include "ps2/render/Ps2SectionVisibility.h"

#include <array>

namespace
{
    constexpr int kVisibilityShortcutOpaqueCount = 256;

    struct VisibilityScratch
    {
        std::array<std::uint64_t, Ps2SectionVisibility::kWordCount> visited{};
        std::array<std::uint16_t, Ps2SectionVisibility::kCellCount> queue{};
    };

    VisibilityScratch s_visibilityScratch;

    int localIndex(int x, int y, int z)
    {
        return (y << 8) | (z << 4) | x;
    }

    bool isOpaque(const std::array<std::uint64_t, Ps2SectionVisibility::kWordCount> &opaqueBits, int index)
    {
        return (opaqueBits[static_cast<std::size_t>(index >> 6)] &
            (std::uint64_t{1} << (index & 63))) != 0;
    }

    std::uint8_t boundaryFaces(int x, int y, int z)
    {
        std::uint8_t faces = 0;
        if (y == 0) faces |= 1u << Ps2SectionVisibility::Down;
        if (y == 15) faces |= 1u << Ps2SectionVisibility::Up;
        if (z == 0) faces |= 1u << Ps2SectionVisibility::North;
        if (z == 15) faces |= 1u << Ps2SectionVisibility::South;
        if (x == 0) faces |= 1u << Ps2SectionVisibility::West;
        if (x == 15) faces |= 1u << Ps2SectionVisibility::East;
        return faces;
    }
}

Ps2SectionVisibility::Ps2SectionVisibility()
{
    setAllVisible();
}

void Ps2SectionVisibility::build(
    const std::array<std::uint64_t, kWordCount> &opaqueBits, int opaqueCount)
{
    visibleByFace.fill(0);

    if (opaqueCount <= 0 || opaqueCount < kVisibilityShortcutOpaqueCount)
    {
        setAllVisible();
        return;
    }
    if (opaqueCount >= kCellCount)
        return;

    s_visibilityScratch.visited.fill(0);
    auto &visited = s_visibilityScratch.visited;
    auto &queue = s_visibilityScratch.queue;

    auto wasVisited = [&](int index) -> bool
    {
        return (visited[static_cast<std::size_t>(index >> 6)] &
            (std::uint64_t{1} << (index & 63))) != 0;
    };
    auto markVisited = [&](int index)
    {
        visited[static_cast<std::size_t>(index >> 6)] |=
            std::uint64_t{1} << (index & 63);
    };

    for (int y = 0; y < kSectionSize; ++y)
    {
        for (int z = 0; z < kSectionSize; ++z)
        {
            for (int x = 0; x < kSectionSize; ++x)
            {
                const std::uint8_t startFaces = boundaryFaces(x, y, z);
                if (startFaces == 0)
                    continue;

                const int startIndex = localIndex(x, y, z);
                if (isOpaque(opaqueBits, startIndex) || wasVisited(startIndex))
                    continue;

                int queueRead = 0;
                int queueWrite = 0;
                std::uint8_t componentFaces = 0;
                queue[static_cast<std::size_t>(queueWrite++)] = static_cast<std::uint16_t>(startIndex);
                markVisited(startIndex);

                while (queueRead < queueWrite)
                {
                    const int index = queue[static_cast<std::size_t>(queueRead++)];
                    const int cellX = index & 15;
                    const int cellZ = (index >> 4) & 15;
                    const int cellY = (index >> 8) & 15;
                    componentFaces |= boundaryFaces(cellX, cellY, cellZ);

                    const int neighbours[kFaceCount] = {
                        index - 256, index + 256,
                        index - 16, index + 16,
                        index - 1, index + 1
                    };
                    const bool valid[kFaceCount] = {
                        cellY > 0, cellY < 15,
                        cellZ > 0, cellZ < 15,
                        cellX > 0, cellX < 15
                    };

                    for (int side = 0; side < kFaceCount; ++side)
                    {
                        if (!valid[side])
                            continue;
                        const int neighbour = neighbours[side];
                        if (isOpaque(opaqueBits, neighbour) || wasVisited(neighbour))
                            continue;
                        markVisited(neighbour);
                        queue[static_cast<std::size_t>(queueWrite++)] = static_cast<std::uint16_t>(neighbour);
                    }
                }

                setComponentVisible(componentFaces);
            }
        }
    }
}

std::uint8_t Ps2SectionVisibility::visibleFacesFrom(int face) const
{
    if (face < 0 || face >= kFaceCount)
        return kAllFaces;
    return visibleByFace[static_cast<std::size_t>(face)];
}

void Ps2SectionVisibility::setAllVisible()
{
    visibleByFace.fill(kAllFaces);
}

void Ps2SectionVisibility::setComponentVisible(std::uint8_t faces)
{
    for (int face = 0; face < kFaceCount; ++face)
    {
        if ((faces & (1u << face)) != 0)
            visibleByFace[static_cast<std::size_t>(face)] |= faces;
    }
}
