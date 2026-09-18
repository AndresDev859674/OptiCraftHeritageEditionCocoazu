#pragma once

#ifdef PS2_PLATFORM

#include "java/Type.h"

enum class Ps2Vu0MeshFinalizePollResult
{
    Idle,
    Pending,
    Complete,
    Failed
};

// VU0 micro-mode accelerator for the numeric half of terrain-mesh packing.
// begin() launches one batch and returns while VU0 is still running; callers
// may do scalar EE work until poll()/drain(), but must not issue VU0 macro
// instructions while poll() still reports Pending.
bool ps2_vu0_mesh_finalize_begin(const int_t* raw, int_t vertexCount);
Ps2Vu0MeshFinalizePollResult ps2_vu0_mesh_finalize_poll();
bool ps2_vu0_mesh_finalize_wait();
bool ps2_vu0_mesh_finalize_drain();
const volatile int_t* ps2_vu0_mesh_finalize_result();
int_t ps2_vu0_mesh_finalize_batch_capacity();

#endif // PS2_PLATFORM
