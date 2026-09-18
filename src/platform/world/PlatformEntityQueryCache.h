#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <typeinfo>
#include <vector>

template <typename T, std::size_t SlotCount = 6>
class PlatformEntityQueryCache
{
public:
    template <typename Predicate>
    const std::vector<T*> &getOrBuild(const std::type_info &type,
                                      std::uint32_t revision,
                                      const std::vector<T*> &source,
                                      Predicate predicate)
    {
        Entry *entry = find(type);
        if (entry == nullptr)
            entry = selectReplacement();

        entry->stamp = ++stampCounter;
        if (entry->type != nullptr && *entry->type == type && entry->revision == revision)
            return entry->members;

        entry->type = &type;
        entry->revision = revision;
        entry->members.clear();
        entry->members.reserve(source.size());
        for (T *value : source)
        {
            if (value != nullptr && predicate(value, type))
                entry->members.push_back(value);
        }
        return entry->members;
    }

    void invalidate()
    {
        for (Entry &entry : entries)
        {
            entry.type = nullptr;
            entry.revision = 0;
            entry.stamp = 0;
            entry.members.clear();
        }
    }

private:
    struct Entry
    {
        const std::type_info *type = nullptr;
        std::uint32_t revision = 0;
        std::uint32_t stamp = 0;
        std::vector<T*> members;
    };

    Entry *find(const std::type_info &type)
    {
        for (Entry &entry : entries)
        {
            if (entry.type != nullptr && *entry.type == type)
                return &entry;
        }
        return nullptr;
    }

    Entry *selectReplacement()
    {
        Entry *oldest = &entries[0];
        for (Entry &entry : entries)
        {
            if (entry.type == nullptr)
                return &entry;
            if (entry.stamp < oldest->stamp)
                oldest = &entry;
        }
        return oldest;
    }

    std::array<Entry, SlotCount> entries{};
    std::uint32_t stampCounter = 0;
};
