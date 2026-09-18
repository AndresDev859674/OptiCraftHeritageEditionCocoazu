#include "ps2/render/Ps2SectionOcclusion.h"

#include <algorithm>
#include <array>

namespace
{
    struct OcclusionScratch
    {
        std::array<std::uint8_t, Ps2SectionOcclusion::kMaxSections> entryMasks{};
        std::array<std::uint16_t, Ps2SectionOcclusion::kMaxSections * Ps2SectionOcclusion::kFaceCount> queue{};
    };

    OcclusionScratch s_occlusionScratch;

    constexpr int kOppositeFace[Ps2SectionOcclusion::kFaceCount] = {
        Ps2SectionOcclusion::Up,
        Ps2SectionOcclusion::Down,
        Ps2SectionOcclusion::South,
        Ps2SectionOcclusion::North,
        Ps2SectionOcclusion::East,
        Ps2SectionOcclusion::West
    };
}

void Ps2SectionOcclusion::compute(int sectionCount,
    int cameraIndex,
    const std::array<std::int16_t, kFaceCount> *neighbours,
    const std::array<std::uint8_t, kFaceCount> *visibleFaces,
    bool *outVisible)
{
    if (outVisible == nullptr || sectionCount <= 0)
        return;

    const int boundedCount = std::min(sectionCount, kMaxSections);
    std::fill(outVisible, outVisible + sectionCount, true);
    if (sectionCount > kMaxSections || cameraIndex < 0 || cameraIndex >= boundedCount ||
        neighbours == nullptr || visibleFaces == nullptr)
    {
        return;
    }

    std::fill(outVisible, outVisible + boundedCount, false);
    s_occlusionScratch.entryMasks.fill(0);
    auto &entryMasks = s_occlusionScratch.entryMasks;
    auto &queue = s_occlusionScratch.queue;
    int queueRead = 0;
    int queueWrite = 0;

    outVisible[cameraIndex] = true;

    auto enqueueNeighbour = [&](int rendererIndex, int exitFace)
    {
        if (rendererIndex < 0 || rendererIndex >= boundedCount || exitFace < 0 || exitFace >= kFaceCount)
            return;

        const int neighbourIndex = neighbours[rendererIndex][static_cast<std::size_t>(exitFace)];
        if (neighbourIndex < 0 || neighbourIndex >= boundedCount)
            return;

        const int entryFace = kOppositeFace[exitFace];
        const std::uint8_t entryBit = static_cast<std::uint8_t>(1u << entryFace);
        std::uint8_t &entryMask = entryMasks[static_cast<std::size_t>(neighbourIndex)];
        if ((entryMask & entryBit) != 0)
            return;

        entryMask |= entryBit;
        outVisible[neighbourIndex] = true;
        if (queueWrite < static_cast<int>(queue.size()))
        {
            queue[static_cast<std::size_t>(queueWrite++)] = static_cast<std::uint16_t>(
                neighbourIndex * kFaceCount + entryFace);
        }
    };

    for (int exitFace = 0; exitFace < kFaceCount; ++exitFace)
        enqueueNeighbour(cameraIndex, exitFace);

    while (queueRead < queueWrite)
    {
        const int encoded = queue[static_cast<std::size_t>(queueRead++)];
        const int rendererIndex = encoded / kFaceCount;
        const int entryFace = encoded % kFaceCount;
        if (rendererIndex < 0 || rendererIndex >= boundedCount)
            continue;

        const std::uint8_t exits = visibleFaces[rendererIndex][static_cast<std::size_t>(entryFace)];
        for (int exitFace = 0; exitFace < kFaceCount; ++exitFace)
        {
            if ((exits & (1u << exitFace)) != 0)
                enqueueNeighbour(rendererIndex, exitFace);
        }
    }
}
