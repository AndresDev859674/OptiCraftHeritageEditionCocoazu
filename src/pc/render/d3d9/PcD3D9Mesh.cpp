#include "pc/render/d3d9/PcD3D9Mesh.h"

#if PLATFORM_PC && defined(MC_WIN32)

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>

#include "pc/render/d3d9/PcD3D9Internal.h"

namespace
{
struct VertexBasic
{
    float x, y, z;
    D3DCOLOR color;
    float u0, v0;
    float u1, v1;
};

struct VertexNormal
{
    float x, y, z;
    float nx, ny, nz;
    D3DCOLOR color;
    float u0, v0;
    float u1, v1;
};

D3DCOLOR readColor(const unsigned char* source)
{
    return D3DCOLOR_ARGB(source[3], source[0], source[1], source[2]);
}

void readPosition(const unsigned char* source, bool positionShort, float& x, float& y, float& z)
{
    if (positionShort)
    {
        const std::int16_t* values = reinterpret_cast<const std::int16_t*>(source);
        x = static_cast<float>(values[0]);
        y = static_cast<float>(values[1]);
        z = static_cast<float>(values[2]);
        return;
    }

    const float* values = reinterpret_cast<const float*>(source);
    x = values[0];
    y = values[1];
    z = values[2];
}

void readNormal(const unsigned char* source, const RenderInterleavedMesh& mesh,
                const float* fallbackNormal, float& x, float& y, float& z)
{
    if (mesh.hasNormals)
    {
        const std::int8_t* values = reinterpret_cast<const std::int8_t*>(source + mesh.normalOffset);
        x = static_cast<float>(values[0]) / 127.0f;
        y = static_cast<float>(values[1]) / 127.0f;
        z = static_cast<float>(values[2]) / 127.0f;
        return;
    }

    x = fallbackNormal != nullptr ? fallbackNormal[0] : 0.0f;
    y = fallbackNormal != nullptr ? fallbackNormal[1] : 1.0f;
    z = fallbackNormal != nullptr ? fallbackNormal[2] : 0.0f;
}

void readTexCoord(const unsigned char* source, const RenderInterleavedMesh& mesh, float& u, float& v)
{
    if (!mesh.hasTexture)
    {
        u = 0.0f;
        v = 0.0f;
        return;
    }
    const float* values = reinterpret_cast<const float*>(source + mesh.texCoordOffset);
    u = values[0];
    v = values[1];
}

void readBrightness(const unsigned char* source, const RenderInterleavedMesh& mesh, float& u, float& v)
{
    if (!mesh.hasBrightness)
    {
        u = 0.0f;
        v = 0.0f;
        return;
    }
    const std::int16_t* values = reinterpret_cast<const std::int16_t*>(source + mesh.brightnessOffset);
    u = static_cast<float>(values[0]);
    v = static_cast<float>(values[1]);
}

D3DPRIMITIVETYPE primitiveType(RenderPrimitive primitive)
{
    switch (primitive)
    {
        case RenderPrimitive::Points: return D3DPT_POINTLIST;
        case RenderPrimitive::Lines: return D3DPT_LINELIST;
        case RenderPrimitive::LineStrip: return D3DPT_LINESTRIP;
        case RenderPrimitive::TriangleStrip: return D3DPT_TRIANGLESTRIP;
        case RenderPrimitive::TriangleFan: return D3DPT_TRIANGLEFAN;
        case RenderPrimitive::Triangles:
        case RenderPrimitive::Quads:
        case RenderPrimitive::LineLoop:
        default: return D3DPT_TRIANGLELIST;
    }
}

UINT primitiveCount(D3DPRIMITIVETYPE primitive, int vertexCount)
{
    if (vertexCount <= 0)
        return 0;
    switch (primitive)
    {
        case D3DPT_POINTLIST: return static_cast<UINT>(vertexCount);
        case D3DPT_LINELIST: return static_cast<UINT>(vertexCount / 2);
        case D3DPT_LINESTRIP: return static_cast<UINT>(std::max(vertexCount - 1, 0));
        case D3DPT_TRIANGLESTRIP:
        case D3DPT_TRIANGLEFAN: return static_cast<UINT>(std::max(vertexCount - 2, 0));
        case D3DPT_TRIANGLELIST:
        default: return static_cast<UINT>(vertexCount / 3);
    }
}

int outputVertexCount(RenderPrimitive primitive, int count)
{
    if (count <= 0)
        return 0;
    if (primitive == RenderPrimitive::Quads)
        return (count / 4) * 4;
    if (primitive == RenderPrimitive::LineLoop)
        return count * 2;
    return count;
}

int sourceIndexForOutput(RenderPrimitive primitive, int outputIndex)
{
    if (primitive == RenderPrimitive::LineLoop)
    {
        const int edge = outputIndex / 2;
        return (outputIndex & 1) == 0 ? edge : edge + 1;
    }
    return outputIndex;
}

IDirect3DVertexBuffer9* g_dynamicVertexBuffer = nullptr;
UINT g_dynamicVertexCapacity = 0;
IDirect3DIndexBuffer9* g_quadIndexBuffer = nullptr;

constexpr UINT kMaxQuadsPerIndexedDraw = 16383u;

bool ensureQuadIndexBuffer(IDirect3DDevice9* device)
{
    if (device == nullptr)
        return false;
    if (g_quadIndexBuffer != nullptr)
        return true;

    const UINT indexCount = kMaxQuadsPerIndexedDraw * 6u;
    if (FAILED(device->CreateIndexBuffer(indexCount * static_cast<UINT>(sizeof(std::uint16_t)),
                                         D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED,
                                         &g_quadIndexBuffer, nullptr)))
        return false;

    void* destination = nullptr;
    if (FAILED(g_quadIndexBuffer->Lock(0, 0, &destination, 0)) || destination == nullptr)
    {
        g_quadIndexBuffer->Release();
        g_quadIndexBuffer = nullptr;
        return false;
    }

    std::uint16_t* indices = static_cast<std::uint16_t*>(destination);
    for (UINT quad = 0; quad < kMaxQuadsPerIndexedDraw; ++quad)
    {
        const std::uint16_t base = static_cast<std::uint16_t>(quad * 4u);
        const UINT index = quad * 6u;
        indices[index + 0u] = base;
        indices[index + 1u] = static_cast<std::uint16_t>(base + 1u);
        indices[index + 2u] = static_cast<std::uint16_t>(base + 2u);
        indices[index + 3u] = base;
        indices[index + 4u] = static_cast<std::uint16_t>(base + 2u);
        indices[index + 5u] = static_cast<std::uint16_t>(base + 3u);
    }
    g_quadIndexBuffer->Unlock();
    pcD3D9InvalidateVertexBindings();
    return true;
}

bool drawIndexedQuads(IDirect3DDevice9* device, int vertexCount)
{
    if (device == nullptr || vertexCount < 4 || !ensureQuadIndexBuffer(device) ||
        !pcD3D9SetIndexBuffer(g_quadIndexBuffer))
        return false;

    UINT remainingQuads = static_cast<UINT>(vertexCount / 4);
    UINT baseVertex = 0;
    while (remainingQuads > 0)
    {
        const UINT batchQuads = std::min(remainingQuads, kMaxQuadsPerIndexedDraw);
        const UINT batchVertices = batchQuads * 4u;
        if (FAILED(device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,
                                                static_cast<INT>(baseVertex), 0, batchVertices,
                                                0, batchQuads * 2u)))
            return false;
        baseVertex += batchVertices;
        remainingQuads -= batchQuads;
    }
    return true;
}

UINT dynamicCapacityFor(UINT required)
{
    UINT capacity = 64u * 1024u;
    while (capacity < required && capacity <= 0x40000000u)
        capacity *= 2u;
    return std::max(capacity, required);
}

bool ensureDynamicVertexBuffer(IDirect3DDevice9* device, UINT required)
{
    if (device == nullptr || required == 0)
        return false;
    if (g_dynamicVertexBuffer != nullptr && g_dynamicVertexCapacity >= required)
        return true;

    pcD3D9ReleaseDynamicMeshBuffer();
    g_dynamicVertexCapacity = dynamicCapacityFor(required);
    if (FAILED(device->CreateVertexBuffer(g_dynamicVertexCapacity,
                                          D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0,
                                          D3DPOOL_DEFAULT, &g_dynamicVertexBuffer, nullptr)))
    {
        g_dynamicVertexCapacity = 0;
        return false;
    }
    pcD3D9InvalidateVertexBindings();
    return true;
}

bool compatibleForMerge(const PcD3D9RetainedMesh& left, const PcD3D9RetainedMesh& right)
{
    return left.fvf == right.fvf && left.stride == right.stride &&
           left.primitive == right.primitive && left.hasTexture == right.hasTexture &&
           left.hasColor == right.hasColor && left.hasNormals == right.hasNormals &&
           left.hasBrightness == right.hasBrightness && left.indexedQuads == right.indexedQuads;
}

void transformPosition(unsigned char* vertex, const PcD3D9Matrix& transform)
{
    float* position = reinterpret_cast<float*>(vertex);
    const float x = position[0];
    const float y = position[1];
    const float z = position[2];
    position[0] = transform.values[0] * x + transform.values[4] * y + transform.values[8] * z + transform.values[12];
    position[1] = transform.values[1] * x + transform.values[5] * y + transform.values[9] * z + transform.values[13];
    position[2] = transform.values[2] * x + transform.values[6] * y + transform.values[10] * z + transform.values[14];
}
}

PcD3D9RetainedMesh::~PcD3D9RetainedMesh()
{
    if (vertexBuffer != nullptr)
        vertexBuffer->Release();
}

bool pcD3D9PrepareMesh(const RenderInterleavedMesh& mesh, const float* fallbackNormal, PcD3D9PreparedMesh& out)
{
    out.vertices.clear();
    out.fvf = 0;
    out.stride = 0;
    out.primitive = D3DPT_TRIANGLELIST;
    out.primitiveCount = 0;
    out.vertexCount = 0;
    out.hasTexture = false;
    out.hasColor = false;
    out.hasNormals = false;
    out.hasBrightness = false;
    out.indexedQuads = false;
    if (mesh.data == nullptr || mesh.stride <= 0 || mesh.count <= 0 || mesh.first < 0)
        return false;

    const int convertedVertexCount = outputVertexCount(mesh.primitive, mesh.count);
    if (convertedVertexCount <= 0)
        return false;

    const bool useNormals = mesh.hasNormals || fallbackNormal != nullptr;
    out.stride = useNormals ? static_cast<UINT>(sizeof(VertexNormal)) : static_cast<UINT>(sizeof(VertexBasic));
    out.fvf = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2;
    if (useNormals)
        out.fvf |= D3DFVF_NORMAL;
    out.indexedQuads = mesh.primitive == RenderPrimitive::Quads;
    out.primitive = mesh.primitive == RenderPrimitive::LineLoop
        ? D3DPT_LINELIST
        : primitiveType(mesh.primitive);
    out.vertexCount = convertedVertexCount;
    out.primitiveCount = out.indexedQuads
        ? static_cast<UINT>((out.vertexCount / 4) * 2)
        : primitiveCount(out.primitive, out.vertexCount);
    out.hasTexture = mesh.hasTexture;
    out.hasColor = mesh.hasColor;
    out.hasNormals = useNormals;
    out.hasBrightness = mesh.hasBrightness;
    out.vertices.resize(static_cast<std::size_t>(out.vertexCount) * out.stride);

    const unsigned char* base = static_cast<const unsigned char*>(mesh.data) +
                                static_cast<std::size_t>(mesh.first) * static_cast<std::size_t>(mesh.stride);

    for (int outputIndex = 0; outputIndex < out.vertexCount; ++outputIndex)
    {
        int sourceIndex = sourceIndexForOutput(mesh.primitive, outputIndex);
        if (mesh.primitive == RenderPrimitive::LineLoop && sourceIndex == mesh.count)
            sourceIndex = 0;
        const unsigned char* source = base + static_cast<std::size_t>(sourceIndex) * static_cast<std::size_t>(mesh.stride);

        float x = 0.0f, y = 0.0f, z = 0.0f;
        float nx = 0.0f, ny = 1.0f, nz = 0.0f;
        float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
        readPosition(source, mesh.positionShort, x, y, z);
        readNormal(source, mesh, fallbackNormal, nx, ny, nz);
        readTexCoord(source, mesh, u0, v0);
        readBrightness(source, mesh, u1, v1);
        const D3DCOLOR color = mesh.hasColor
            ? readColor(source + mesh.colorOffset)
            : D3DCOLOR_ARGB(255, 255, 255, 255);

        unsigned char* destination = out.vertices.data() + static_cast<std::size_t>(outputIndex) * out.stride;
        if (useNormals)
        {
            VertexNormal vertex{x, y, z, nx, ny, nz, color, u0, v0, u1, v1};
            std::memcpy(destination, &vertex, sizeof(vertex));
        }
        else
        {
            VertexBasic vertex{x, y, z, color, u0, v0, u1, v1};
            std::memcpy(destination, &vertex, sizeof(vertex));
        }
    }

    return out.primitiveCount > 0;
}

std::shared_ptr<PcD3D9RetainedMesh> pcD3D9CreateRetainedMesh(IDirect3DDevice9* device,
                                                              const RenderInterleavedMesh& mesh,
                                                              const float* fallbackNormal)
{
    if (device == nullptr)
        return {};

    static PcD3D9PreparedMesh prepared;
    if (!pcD3D9PrepareMesh(mesh, fallbackNormal, prepared))
        return {};

    auto retained = std::make_shared<PcD3D9RetainedMesh>();
    retained->fvf = prepared.fvf;
    retained->stride = prepared.stride;
    retained->primitive = prepared.primitive;
    retained->primitiveCount = prepared.primitiveCount;
    retained->vertexCount = prepared.vertexCount;
    retained->hasTexture = prepared.hasTexture;
    retained->hasColor = prepared.hasColor;
    retained->hasNormals = prepared.hasNormals;
    retained->hasBrightness = prepared.hasBrightness;
    retained->indexedQuads = prepared.indexedQuads;

    if (FAILED(device->CreateVertexBuffer(static_cast<UINT>(prepared.vertices.size()), 0, prepared.fvf,
                                          D3DPOOL_MANAGED, &retained->vertexBuffer, nullptr)))
        return {};

    void* destination = nullptr;
    if (FAILED(retained->vertexBuffer->Lock(0, 0, &destination, 0)) || destination == nullptr)
        return {};
    std::memcpy(destination, prepared.vertices.data(), prepared.vertices.size());
    retained->vertexBuffer->Unlock();
    return retained;
}

std::shared_ptr<PcD3D9RetainedMesh> pcD3D9CreateTransformedRetainedMesh(IDirect3DDevice9* device,
                                                                         const PcD3D9RetainedMesh& source,
                                                                         const PcD3D9Matrix& transform)
{
    if (device == nullptr || source.vertexBuffer == nullptr || source.vertexCount <= 0 ||
        source.stride < static_cast<UINT>(sizeof(float) * 3u) || source.hasNormals)
        return {};

    const std::size_t byteCount = static_cast<std::size_t>(source.vertexCount) * source.stride;
    if (byteCount == 0 || byteCount > static_cast<std::size_t>(UINT_MAX))
        return {};

    auto transformed = std::make_shared<PcD3D9RetainedMesh>();
    transformed->fvf = source.fvf;
    transformed->stride = source.stride;
    transformed->primitive = source.primitive;
    transformed->primitiveCount = source.primitiveCount;
    transformed->vertexCount = source.vertexCount;
    transformed->hasTexture = source.hasTexture;
    transformed->hasColor = source.hasColor;
    transformed->hasNormals = source.hasNormals;
    transformed->hasBrightness = source.hasBrightness;
    transformed->indexedQuads = source.indexedQuads;

    if (FAILED(device->CreateVertexBuffer(static_cast<UINT>(byteCount), 0, source.fvf,
                                          D3DPOOL_MANAGED, &transformed->vertexBuffer, nullptr)))
        return {};

    void* sourceData = nullptr;
    void* destinationData = nullptr;
    if (FAILED(source.vertexBuffer->Lock(0, 0, &sourceData, D3DLOCK_READONLY)) || sourceData == nullptr)
        return {};
    if (FAILED(transformed->vertexBuffer->Lock(0, 0, &destinationData, 0)) || destinationData == nullptr)
    {
        source.vertexBuffer->Unlock();
        return {};
    }

    std::memcpy(destinationData, sourceData, byteCount);
    unsigned char* vertices = static_cast<unsigned char*>(destinationData);
    for (int vertex = 0; vertex < source.vertexCount; ++vertex)
        transformPosition(vertices + static_cast<std::size_t>(vertex) * source.stride, transform);

    transformed->vertexBuffer->Unlock();
    source.vertexBuffer->Unlock();
    return transformed;
}

std::shared_ptr<PcD3D9RetainedMesh> pcD3D9CreateMergedRetainedMesh(
    IDirect3DDevice9* device, const std::vector<const PcD3D9RetainedMesh*>& meshes)
{
    if (device == nullptr || meshes.size() < 2 || meshes.front() == nullptr)
        return {};

    const PcD3D9RetainedMesh& first = *meshes.front();
    if (first.vertexBuffer == nullptr || !first.indexedQuads || first.vertexCount <= 0)
        return {};

    std::size_t totalBytes = 0;
    std::size_t totalVertices = 0;
    for (const PcD3D9RetainedMesh* mesh : meshes)
    {
        if (mesh == nullptr || mesh->vertexBuffer == nullptr || mesh->vertexCount <= 0 ||
            !compatibleForMerge(first, *mesh))
            return {};
        totalBytes += static_cast<std::size_t>(mesh->vertexCount) * mesh->stride;
        totalVertices += static_cast<std::size_t>(mesh->vertexCount);
        if (totalBytes > static_cast<std::size_t>(UINT_MAX) || totalVertices > static_cast<std::size_t>(INT_MAX))
            return {};
    }

    auto merged = std::make_shared<PcD3D9RetainedMesh>();
    merged->fvf = first.fvf;
    merged->stride = first.stride;
    merged->primitive = first.primitive;
    merged->vertexCount = static_cast<int>(totalVertices);
    merged->primitiveCount = static_cast<UINT>((totalVertices / 4u) * 2u);
    merged->hasTexture = first.hasTexture;
    merged->hasColor = first.hasColor;
    merged->hasNormals = first.hasNormals;
    merged->hasBrightness = first.hasBrightness;
    merged->indexedQuads = first.indexedQuads;

    if (FAILED(device->CreateVertexBuffer(static_cast<UINT>(totalBytes), 0, first.fvf,
                                          D3DPOOL_MANAGED, &merged->vertexBuffer, nullptr)))
        return {};

    void* destinationData = nullptr;
    if (FAILED(merged->vertexBuffer->Lock(0, 0, &destinationData, 0)) || destinationData == nullptr)
        return {};

    unsigned char* destination = static_cast<unsigned char*>(destinationData);
    std::size_t destinationOffset = 0;
    for (const PcD3D9RetainedMesh* mesh : meshes)
    {
        const std::size_t bytes = static_cast<std::size_t>(mesh->vertexCount) * mesh->stride;
        void* sourceData = nullptr;
        if (FAILED(mesh->vertexBuffer->Lock(0, 0, &sourceData, D3DLOCK_READONLY)) || sourceData == nullptr)
        {
            merged->vertexBuffer->Unlock();
            return {};
        }
        std::memcpy(destination + destinationOffset, sourceData, bytes);
        mesh->vertexBuffer->Unlock();
        destinationOffset += bytes;
    }

    merged->vertexBuffer->Unlock();
    return merged;
}

bool pcD3D9DrawPreparedMesh(IDirect3DDevice9* device, const PcD3D9PreparedMesh& mesh)
{
    if (device == nullptr || mesh.vertices.empty() || mesh.primitiveCount == 0)
        return false;

    const UINT bytes = static_cast<UINT>(mesh.vertices.size());
    if (!ensureDynamicVertexBuffer(device, bytes))
        return false;

    void* destination = nullptr;
    if (FAILED(g_dynamicVertexBuffer->Lock(0, bytes, &destination, D3DLOCK_DISCARD)) || destination == nullptr)
        return false;
    std::memcpy(destination, mesh.vertices.data(), bytes);
    g_dynamicVertexBuffer->Unlock();

    if (!pcD3D9SetFvf(mesh.fvf) || !pcD3D9SetVertexStream(g_dynamicVertexBuffer, mesh.stride))
        return false;
    if (mesh.indexedQuads)
        return drawIndexedQuads(device, mesh.vertexCount);
    return SUCCEEDED(device->DrawPrimitive(mesh.primitive, 0, mesh.primitiveCount));
}

bool pcD3D9DrawRetainedMesh(IDirect3DDevice9* device, const PcD3D9RetainedMesh& mesh)
{
    if (device == nullptr || mesh.vertexBuffer == nullptr || mesh.primitiveCount == 0)
        return false;
    if (!pcD3D9SetFvf(mesh.fvf) || !pcD3D9SetVertexStream(mesh.vertexBuffer, mesh.stride))
        return false;
    if (mesh.indexedQuads)
        return drawIndexedQuads(device, mesh.vertexCount);
    return SUCCEEDED(device->DrawPrimitive(mesh.primitive, 0, mesh.primitiveCount));
}

void pcD3D9ReleaseDynamicMeshBuffer()
{
    if (g_dynamicVertexBuffer != nullptr)
    {
        g_dynamicVertexBuffer->Release();
        g_dynamicVertexBuffer = nullptr;
    }
    g_dynamicVertexCapacity = 0;
    pcD3D9InvalidateVertexBindings();
}

void pcD3D9ReleaseSharedMeshResources()
{
    if (g_quadIndexBuffer != nullptr)
    {
        g_quadIndexBuffer->Release();
        g_quadIndexBuffer = nullptr;
    }
    pcD3D9InvalidateVertexBindings();
}

#endif
