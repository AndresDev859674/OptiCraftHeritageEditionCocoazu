#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

class Ps2EntityPointerIndex
{
public:
    static constexpr std::size_t kCapacity = 512;
    static constexpr std::size_t kMaxEntries = (kCapacity * 3) / 4;

    Ps2EntityPointerIndex();

    bool insert(const void *pointer);
    bool erase(const void *pointer);
    bool contains(const void *pointer) const;
    void clear();
    std::size_t size() const;

private:
    enum SlotState : std::uint8_t
    {
        Empty = 0,
        Occupied = 1,
        Tombstone = 2
    };

    static std::size_t hashPointer(const void *pointer);

    std::array<const void *, kCapacity> pointers{};
    std::array<std::uint8_t, kCapacity> states{};
    std::size_t entryCount = 0;
};
