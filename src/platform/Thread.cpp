#include "platform/Thread.h"

#ifdef WII_PLATFORM
#include <ogc/lwp.h>
#else
#include <thread>
#include <functional>
#include <new>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#endif

#ifdef WII_PLATFORM
PlatformThread::PlatformThread() : handle_(static_cast<std::uintptr_t>(LWP_THREAD_NULL)) {}
PlatformThread::~PlatformThread() { if (joinable() && !isCurrent()) join(); }
bool PlatformThread::start(Entry entry, void* argument, std::size_t stackSize, int priority, std::uintptr_t affinityMask)
{
    if (joinable() || entry == nullptr) return false;
    (void)affinityMask;
    lwp_t thread = LWP_THREAD_NULL;
    const s32 result = LWP_CreateThread(&thread, entry, argument, nullptr, static_cast<u32>(stackSize), priority);
    if (result < 0) return false;
    handle_ = static_cast<std::uintptr_t>(thread);
    return true;
}
void PlatformThread::join()
{
    if (!joinable() || isCurrent()) return;
    LWP_JoinThread(static_cast<lwp_t>(handle_), nullptr);
    handle_ = static_cast<std::uintptr_t>(LWP_THREAD_NULL);
}
bool PlatformThread::joinable() const { return static_cast<lwp_t>(handle_) != LWP_THREAD_NULL; }
bool PlatformThread::isCurrent() const { return joinable() && static_cast<lwp_t>(handle_) == LWP_GetSelf(); }
std::uintptr_t PlatformThread::currentId() { return static_cast<std::uintptr_t>(LWP_GetSelf()); }
#else
struct PlatformThread::Impl { std::thread thread; };
PlatformThread::PlatformThread() : impl_(new (std::nothrow) Impl()) {}
PlatformThread::~PlatformThread() { if (impl_) { if (joinable() && !isCurrent()) join(); delete impl_; } }
bool PlatformThread::start(Entry entry, void* argument, std::size_t, int priority, std::uintptr_t affinityMask)
{
    if (!impl_ || !entry || joinable()) return false;
    try
    {
        impl_->thread = std::thread([entry, argument, priority, affinityMask]()
        {
#if defined(_WIN32)
            if (priority < 64)
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
            else if (priority > 64)
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
            if (affinityMask != 0)
                SetThreadAffinityMask(GetCurrentThread(), static_cast<DWORD_PTR>(affinityMask));
#else
            (void)priority;
            (void)affinityMask;
#endif
            entry(argument);
        });
        return true;
    }
    catch (...)
    {
        return false;
    }
}
void PlatformThread::join() { if (joinable() && !isCurrent()) impl_->thread.join(); }
bool PlatformThread::joinable() const { return impl_ && impl_->thread.joinable(); }
bool PlatformThread::isCurrent() const { return joinable() && impl_->thread.get_id() == std::this_thread::get_id(); }
std::uintptr_t PlatformThread::currentId() { return std::hash<std::thread::id>()(std::this_thread::get_id()); }

#endif
