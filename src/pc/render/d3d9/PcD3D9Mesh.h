#pragma once

#include "platform/PlatformConfig.h"

#if PLATFORM_PC && defined(MC_WIN32)

#include <cstdint>
#include <memory>
#include <vector>

#include <d3d9.h>

#include "platform/RenderAPI.h"

struct PcD3D9Matrix;

struct PcD3D9PreparedMesh
{
    std::vector<unsigned char> vertices;
    DWORD fvf = 0;
    UINT stride = 0;
    D3DPRIMITIVETYPE primitive = D3DPT_TRIANGLELIST;
    UINT primitiveCount = 0;
    int vertexCount = 0;
    bool hasTexture = false;
    bool hasColor = false;
    bool hasNormals = false;
    bool hasBrightness = false;
    bool indexedQuads = false;
};

class PcD3D9RetainedMesh
{
public:
    ~PcD3D9RetainedMesh();

    IDirect3DVertexBuffer9* vertexBuffer = nullptr;
    DWORD fvf = 0;
    UINT stride = 0;
    D3DPRIMITIVETYPE primitive = D3DPT_TRIANGLELIST;
    UINT primitiveCount = 0;
    int vertexCount = 0;
    bool hasTexture = false;
    bool hasColor = false;
    bool hasNormals = false;
    bool hasBrightness = false;
    bool indexedQuads = false;
};

bool pcD3D9PrepareMesh(const RenderInterleavedMesh& mesh, const float* fallbackNormal, PcD3D9PreparedMesh& out);
std::shared_ptr<PcD3D9RetainedMesh> pcD3D9CreateRetainedMesh(IDirect3DDevice9* device,
                                                              const RenderInterleavedMesh& mesh,
                                                              const float* fallbackNormal);
std::shared_ptr<PcD3D9RetainedMesh> pcD3D9CreateTransformedRetainedMesh(IDirect3DDevice9* device,
                                                                         const PcD3D9RetainedMesh& source,
                                                                         const PcD3D9Matrix& transform);
std::shared_ptr<PcD3D9RetainedMesh> pcD3D9CreateMergedRetainedMesh(
    IDirect3DDevice9* device, const std::vector<const PcD3D9RetainedMesh*>& meshes);
bool pcD3D9DrawPreparedMesh(IDirect3DDevice9* device, const PcD3D9PreparedMesh& mesh);
bool pcD3D9DrawRetainedMesh(IDirect3DDevice9* device, const PcD3D9RetainedMesh& mesh);
void pcD3D9ReleaseDynamicMeshBuffer();
void pcD3D9ReleaseSharedMeshResources();

#endif
