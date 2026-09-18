#pragma once

// EE thread priorities: lower number = higher priority, no time slicing. A
// thread only runs while every thread with a lower number is blocked, so any
// background worker that must keep up with hardware has to sit above main.
namespace Ps2ThreadPriority
{
// The kernel starts main at 0 (highest). Lowered in Ps2Bootstrap so I/O-bound
// workers can preempt the frame loop, which busy-waits on vsync.
constexpr int kMain = 64;
// Music stream thread. Blocked on IOP RPC almost all the time; when it wakes it
// pushes one chunk and sleeps again, so preempting main costs microseconds.
constexpr int kMusicStream = 32;
// Background texture asset I/O. It must run above main so it can issue a USB
// request while the frame loop is active, but below music streaming.
constexpr int kAssetIo = 48;
}
