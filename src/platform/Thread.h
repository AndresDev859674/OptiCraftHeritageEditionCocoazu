#pragma once

#include <cstddef>
#include <cstdint>

class PlatformThread
{
public:
    using Entry = void* (*)(void*);

    PlatformThread();
    ~PlatformThread();
    PlatformThread(const PlatformThread&) = delete;
    PlatformThread& operator=(const PlatformThread&) = delete;

    bool start(Entry entry, void* argument, std::size_t stackSize = 32 * 1024,
               int priority = 64, std::uintptr_t affinityMask = 0);
    void join();
    bool joinable() const;
    bool isCurrent() const;

    // Identity of the calling thread, for code that has to tell two threads
    // apart without owning either PlatformThread. Comparable and stable for the
    // lifetime of the thread; the value itself carries no meaning.
    static std::uintptr_t currentId();

private:
#ifdef WII_PLATFORM
    std::uintptr_t handle_;
#else
    struct Impl;
    Impl* impl_;
#endif
};
