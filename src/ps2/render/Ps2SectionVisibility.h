#pragma once

#include <array>
#include <cstdint>

class Ps2SectionVisibility
{
public:
    static constexpr int kSectionSize = 16;
    static constexpr int kCellCount = kSectionSize * kSectionSize * kSectionSize;
    static constexpr int kWordCount = kCellCount / 64;
    static constexpr int kFaceCount = 6;
    static constexpr std::uint8_t kAllFaces = (1u << kFaceCount) - 1u;

    enum Face : int
    {
        Down = 0,
        Up = 1,
        North = 2,
        South = 3,
        West = 4,
        East = 5
    };

    Ps2SectionVisibility();

    void build(const std::array<std::uint64_t, kWordCount> &opaqueBits, int opaqueCount);
    std::uint8_t visibleFacesFrom(int face) const;

private:
    void setAllVisible();
    void setComponentVisible(std::uint8_t faces);

    std::array<std::uint8_t, kFaceCount> visibleByFace{};
};
