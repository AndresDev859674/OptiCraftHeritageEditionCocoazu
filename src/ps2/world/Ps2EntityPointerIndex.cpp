#include "ps2/world/Ps2EntityPointerIndex.h"

#include <cstdint>

static_assert((Ps2EntityPointerIndex::kCapacity & (Ps2EntityPointerIndex::kCapacity - 1)) == 0,
              "Ps2EntityPointerIndex capacity must be a power of two");

Ps2EntityPointerIndex::Ps2EntityPointerIndex()
{
    clear();
}

std::size_t Ps2EntityPointerIndex::hashPointer(const void *pointer)
{
    std::uintptr_t value = reinterpret_cast<std::uintptr_t>(pointer);
    value >>= 4;
    value ^= value >> 16;
    value *= static_cast<std::uintptr_t>(2654435761u);
    return static_cast<std::size_t>(value) & (kCapacity - 1);
}

bool Ps2EntityPointerIndex::insert(const void *pointer)
{
    if (pointer == nullptr)
        return false;

    std::size_t slot = hashPointer(pointer);
    std::size_t firstTombstone = kCapacity;

    for (std::size_t probe = 0; probe < kCapacity; ++probe)
    {
        const std::size_t index = (slot + probe) & (kCapacity - 1);
        const std::uint8_t state = states[index];
        if (state == Occupied)
        {
            if (pointers[index] == pointer)
                return true;
            continue;
        }

        if (state == Tombstone)
        {
            if (firstTombstone == kCapacity)
                firstTombstone = index;
            continue;
        }

        if (entryCount >= kMaxEntries)
            return false;

        const std::size_t destination = firstTombstone != kCapacity ? firstTombstone : index;
        pointers[destination] = pointer;
        states[destination] = Occupied;
        ++entryCount;
        return true;
    }

    if (firstTombstone != kCapacity && entryCount < kMaxEntries)
    {
        pointers[firstTombstone] = pointer;
        states[firstTombstone] = Occupied;
        ++entryCount;
        return true;
    }

    return false;
}

bool Ps2EntityPointerIndex::erase(const void *pointer)
{
    if (pointer == nullptr)
        return false;

    const std::size_t slot = hashPointer(pointer);
    for (std::size_t probe = 0; probe < kCapacity; ++probe)
    {
        const std::size_t index = (slot + probe) & (kCapacity - 1);
        const std::uint8_t state = states[index];
        if (state == Empty)
            return false;
        if (state == Occupied && pointers[index] == pointer)
        {
            pointers[index] = nullptr;
            states[index] = Tombstone;
            --entryCount;
            return true;
        }
    }
    return false;
}

bool Ps2EntityPointerIndex::contains(const void *pointer) const
{
    if (pointer == nullptr)
        return false;

    const std::size_t slot = hashPointer(pointer);
    for (std::size_t probe = 0; probe < kCapacity; ++probe)
    {
        const std::size_t index = (slot + probe) & (kCapacity - 1);
        const std::uint8_t state = states[index];
        if (state == Empty)
            return false;
        if (state == Occupied && pointers[index] == pointer)
            return true;
    }
    return false;
}

void Ps2EntityPointerIndex::clear()
{
    pointers.fill(nullptr);
    states.fill(Empty);
    entryCount = 0;
}

std::size_t Ps2EntityPointerIndex::size() const
{
    return entryCount;
}
