#pragma once

#ifdef PS2_PLATFORM

#include "platform/RenderAPI.h"

// Handle table behind renderCreate/Compile/Draw/DestroyPersistentMesh on PS2.
//
// ModelRenderer asks for a handle before it has any geometry and only compiles
// into it afterwards, so the handle cannot be the cache slot itself:
// Ps2ModelGeometryCache picks the slot inside retain() and deduplicates by
// content, which means two model parts built from identical boxes end up
// sharing one slot. This table owns the identity the caller keeps and points it
// at whichever slot the cache assigned.

int ps2_persistent_mesh_create();
void ps2_persistent_mesh_destroy(int handle);
bool ps2_persistent_mesh_compile(int handle, const RenderInterleavedMesh& mesh);
bool ps2_persistent_mesh_draw(int handle);

#endif // PS2_PLATFORM
