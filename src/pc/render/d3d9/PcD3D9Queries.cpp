#include "pc/render/PcRenderBackendApi.h"

#if PLATFORM_PC && defined(MC_WIN32)

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <unordered_map>

#include "pc/render/d3d9/PcD3D9Context.h"
#include "pc/render/d3d9/PcD3D9Internal.h"

namespace
{
std::unordered_map<int, IDirect3DQuery9*> g_queries;
int g_nextQuery = 1;
int g_activeQuery = 0;

struct SurfacePixel
{
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
    unsigned char a = 255;
};

unsigned char expand4(std::uint16_t value)
{
    return static_cast<unsigned char>(value * 17u);
}

unsigned char expand5(std::uint16_t value)
{
    return static_cast<unsigned char>((value << 3) | (value >> 2));
}

unsigned char expand6(std::uint16_t value)
{
    return static_cast<unsigned char>((value << 2) | (value >> 4));
}

unsigned char expand10(std::uint32_t value)
{
    return static_cast<unsigned char>((value * 255u + 511u) / 1023u);
}

unsigned char expand2(std::uint32_t value)
{
    return static_cast<unsigned char>(value * 85u);
}

bool decodeSurfacePixel(const unsigned char* row, int x, D3DFORMAT format, SurfacePixel* pixel)
{
    if (row == nullptr || x < 0 || pixel == nullptr)
        return false;

    switch (format)
    {
        case D3DFMT_R8G8B8:
        {
            const unsigned char* source = row + static_cast<std::size_t>(x) * 3u;
            pixel->b = source[0];
            pixel->g = source[1];
            pixel->r = source[2];
            pixel->a = 255;
            return true;
        }
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
        {
            const unsigned char* source = row + static_cast<std::size_t>(x) * 4u;
            pixel->b = source[0];
            pixel->g = source[1];
            pixel->r = source[2];
            pixel->a = format == D3DFMT_A8R8G8B8 ? source[3] : 255;
            return true;
        }
        case D3DFMT_A8B8G8R8:
        case D3DFMT_X8B8G8R8:
        {
            const unsigned char* source = row + static_cast<std::size_t>(x) * 4u;
            pixel->r = source[0];
            pixel->g = source[1];
            pixel->b = source[2];
            pixel->a = format == D3DFMT_A8B8G8R8 ? source[3] : 255;
            return true;
        }
        case D3DFMT_R5G6B5:
        {
            std::uint16_t packed = 0;
            std::memcpy(&packed, row + static_cast<std::size_t>(x) * 2u, sizeof(packed));
            pixel->r = expand5(static_cast<std::uint16_t>((packed >> 11) & 0x1fu));
            pixel->g = expand6(static_cast<std::uint16_t>((packed >> 5) & 0x3fu));
            pixel->b = expand5(static_cast<std::uint16_t>(packed & 0x1fu));
            pixel->a = 255;
            return true;
        }
        case D3DFMT_A1R5G5B5:
        case D3DFMT_X1R5G5B5:
        {
            std::uint16_t packed = 0;
            std::memcpy(&packed, row + static_cast<std::size_t>(x) * 2u, sizeof(packed));
            pixel->r = expand5(static_cast<std::uint16_t>((packed >> 10) & 0x1fu));
            pixel->g = expand5(static_cast<std::uint16_t>((packed >> 5) & 0x1fu));
            pixel->b = expand5(static_cast<std::uint16_t>(packed & 0x1fu));
            pixel->a = format == D3DFMT_A1R5G5B5 && (packed & 0x8000u) == 0 ? 0 : 255;
            return true;
        }
        case D3DFMT_A4R4G4B4:
        case D3DFMT_X4R4G4B4:
        {
            std::uint16_t packed = 0;
            std::memcpy(&packed, row + static_cast<std::size_t>(x) * 2u, sizeof(packed));
            pixel->r = expand4(static_cast<std::uint16_t>((packed >> 8) & 0x0fu));
            pixel->g = expand4(static_cast<std::uint16_t>((packed >> 4) & 0x0fu));
            pixel->b = expand4(static_cast<std::uint16_t>(packed & 0x0fu));
            pixel->a = format == D3DFMT_A4R4G4B4
                ? expand4(static_cast<std::uint16_t>((packed >> 12) & 0x0fu))
                : 255;
            return true;
        }
        case D3DFMT_R3G3B2:
        {
            const unsigned char packed = row[x];
            pixel->r = static_cast<unsigned char>(((packed >> 5) & 0x07u) * 255u / 7u);
            pixel->g = static_cast<unsigned char>(((packed >> 2) & 0x07u) * 255u / 7u);
            pixel->b = static_cast<unsigned char>((packed & 0x03u) * 85u);
            pixel->a = 255;
            return true;
        }
        case D3DFMT_A8R3G3B2:
        {
            std::uint16_t packed = 0;
            std::memcpy(&packed, row + static_cast<std::size_t>(x) * 2u, sizeof(packed));
            pixel->r = static_cast<unsigned char>(((packed >> 5) & 0x07u) * 255u / 7u);
            pixel->g = static_cast<unsigned char>(((packed >> 2) & 0x07u) * 255u / 7u);
            pixel->b = static_cast<unsigned char>((packed & 0x03u) * 85u);
            pixel->a = static_cast<unsigned char>((packed >> 8) & 0xffu);
            return true;
        }
        case D3DFMT_A2R10G10B10:
        {
            std::uint32_t packed = 0;
            std::memcpy(&packed, row + static_cast<std::size_t>(x) * 4u, sizeof(packed));
            pixel->b = expand10(packed & 0x3ffu);
            pixel->g = expand10((packed >> 10) & 0x3ffu);
            pixel->r = expand10((packed >> 20) & 0x3ffu);
            pixel->a = expand2((packed >> 30) & 0x3u);
            return true;
        }
        default:
            return false;
    }
}

bool surfaceFormatSupported(D3DFORMAT format)
{
    SurfacePixel pixel{};
    const unsigned char sample[4] = {};
    return decodeSurfacePixel(sample, 0, format, &pixel);
}

void writeA8R8G8B8(unsigned char* destination, const SurfacePixel& pixel)
{
    destination[0] = pixel.b;
    destination[1] = pixel.g;
    destination[2] = pixel.r;
    destination[3] = pixel.a;
}

void releaseQuery(IDirect3DQuery9*& query)
{
    if (query != nullptr)
    {
        query->Release();
        query = nullptr;
    }
}

bool copyRenderTargetToSystemMemory(IDirect3DSurface9** systemSurface, D3DSURFACE_DESC* description)
{
    if (systemSurface == nullptr || description == nullptr)
        return false;
    *systemSurface = nullptr;

    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return false;

    IDirect3DSurface9* renderTarget = nullptr;
    if (FAILED(device->GetRenderTarget(0, &renderTarget)) || renderTarget == nullptr)
        return false;

    const HRESULT descResult = renderTarget->GetDesc(description);
    if (FAILED(descResult))
    {
        renderTarget->Release();
        return false;
    }

    IDirect3DSurface9* readableTarget = renderTarget;
    IDirect3DSurface9* resolvedTarget = nullptr;
    if (description->MultiSampleType != D3DMULTISAMPLE_NONE)
    {
        const HRESULT resolveCreate = device->CreateRenderTarget(description->Width, description->Height,
                                                                  description->Format, D3DMULTISAMPLE_NONE, 0,
                                                                  FALSE, &resolvedTarget, nullptr);
        if (FAILED(resolveCreate) || resolvedTarget == nullptr)
        {
            if (resolvedTarget != nullptr)
                resolvedTarget->Release();
            renderTarget->Release();
            return false;
        }

        const bool sceneWasActive = pcD3D9SceneActive();
        if (sceneWasActive && !pcD3D9SuspendScene())
        {
            resolvedTarget->Release();
            renderTarget->Release();
            return false;
        }
        const HRESULT resolveResult = device->StretchRect(renderTarget, nullptr, resolvedTarget, nullptr, D3DTEXF_NONE);
        const bool resumed = !sceneWasActive || pcD3D9ResumeScene();
        if (FAILED(resolveResult) || !resumed)
        {
            resolvedTarget->Release();
            renderTarget->Release();
            return false;
        }
        readableTarget = resolvedTarget;
    }

    IDirect3DSurface9* copy = nullptr;
    const HRESULT createResult = device->CreateOffscreenPlainSurface(description->Width, description->Height,
                                                                      description->Format, D3DPOOL_SYSTEMMEM,
                                                                      &copy, nullptr);
    if (FAILED(createResult) || copy == nullptr)
    {
        if (resolvedTarget != nullptr)
            resolvedTarget->Release();
        renderTarget->Release();
        return false;
    }

    const HRESULT copyResult = device->GetRenderTargetData(readableTarget, copy);
    if (resolvedTarget != nullptr)
        resolvedTarget->Release();
    renderTarget->Release();
    if (FAILED(copyResult))
    {
        copy->Release();
        return false;
    }

    *systemSurface = copy;
    return true;
}
}

void pcD3D9ClearQueries()
{
    for (auto& entry : g_queries)
        releaseQuery(entry.second);
    g_queries.clear();
    g_nextQuery = 1;
    g_activeQuery = 0;
}

namespace PcD3D9RenderBackend
{
void renderGenerateOcclusionQueries(int count, int* queries)
{
    if (count <= 0 || queries == nullptr)
        return;
    IDirect3DDevice9* device = pcD3D9Device();
    for (int i = 0; i < count; ++i)
    {
        const int id = g_nextQuery++;
        queries[i] = id;
        IDirect3DQuery9* query = nullptr;
        if (device != nullptr && SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_OCCLUSION, &query)) && query != nullptr)
            g_queries[id] = query;
        else
            g_queries[id] = nullptr;
    }
}

void renderBeginOcclusionQuery(int query)
{
    const auto it = g_queries.find(query);
    if (it == g_queries.end() || it->second == nullptr)
        return;
    it->second->Issue(D3DISSUE_BEGIN);
    g_activeQuery = query;
}

void renderEndOcclusionQuery()
{
    const auto it = g_queries.find(g_activeQuery);
    if (it != g_queries.end() && it->second != nullptr)
        it->second->Issue(D3DISSUE_END);
    g_activeQuery = 0;
}

bool renderOcclusionQueryResultAvailable(int query)
{
    const auto it = g_queries.find(query);
    if (it == g_queries.end() || it->second == nullptr)
        return true;
    DWORD pixels = 0;
    return it->second->GetData(&pixels, sizeof(pixels), 0) == S_OK;
}

unsigned int renderOcclusionQueryResult(int query)
{
    const auto it = g_queries.find(query);
    if (it == g_queries.end() || it->second == nullptr)
        return 1u;
    DWORD pixels = 1;
    const HRESULT result = it->second->GetData(&pixels, sizeof(pixels), D3DGETDATA_FLUSH);
    return result == S_OK ? static_cast<unsigned int>(pixels) : 1u;
}

bool renderReadPixelsRgb(int x, int y, int width, int height, void* pixels)
{
    if (pixels == nullptr || x < 0 || y < 0 || width <= 0 || height <= 0)
        return false;

    IDirect3DSurface9* surface = nullptr;
    D3DSURFACE_DESC description{};
    if (!copyRenderTargetToSystemMemory(&surface, &description))
        return false;

    if (x + width > static_cast<int>(description.Width) || y + height > static_cast<int>(description.Height))
    {
        surface->Release();
        return false;
    }

    if (!surfaceFormatSupported(description.Format))
    {
        surface->Release();
        return false;
    }

    D3DLOCKED_RECT locked{};
    if (FAILED(surface->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
    {
        surface->Release();
        return false;
    }

    unsigned char* output = static_cast<unsigned char*>(pixels);
    const int top = static_cast<int>(description.Height) - y - height;
    bool success = true;
    for (int outputY = 0; outputY < height && success; ++outputY)
    {
        // glReadPixels returns rows starting at the requested bottom-left row.
        const int sourceY = top + (height - outputY - 1);
        const unsigned char* sourceRow = static_cast<const unsigned char*>(locked.pBits) +
                                         static_cast<std::size_t>(sourceY) * static_cast<std::size_t>(locked.Pitch);
        unsigned char* destination = output + static_cast<std::size_t>(outputY) *
                                      static_cast<std::size_t>(width) * 3u;
        for (int outputX = 0; outputX < width; ++outputX)
        {
            SurfacePixel pixel{};
            if (!decodeSurfacePixel(sourceRow, x + outputX, description.Format, &pixel))
            {
                success = false;
                break;
            }
            unsigned char* dst = destination + static_cast<std::size_t>(outputX) * 3u;
            dst[0] = pixel.r;
            dst[1] = pixel.g;
            dst[2] = pixel.b;
        }
    }

    surface->UnlockRect();
    surface->Release();
    return success;
}

bool renderCopyFramebufferToBoundTexture(int x, int y, int width, int height)
{
    if (x < 0 || y < 0 || width <= 0 || height <= 0)
        return false;
    auto& state = pcD3D9State();
    IDirect3DTexture9* texture = pcD3D9LookupTexture(state.boundTextureIds[static_cast<std::size_t>(state.activeTextureUnit)]);
    if (texture == nullptr)
        return false;

    IDirect3DSurface9* source = nullptr;
    D3DSURFACE_DESC description{};
    if (!copyRenderTargetToSystemMemory(&source, &description))
        return false;
    if (x + width > static_cast<int>(description.Width) || y + height > static_cast<int>(description.Height))
    {
        source->Release();
        return false;
    }

    if (!surfaceFormatSupported(description.Format))
    {
        source->Release();
        return false;
    }

    D3DSURFACE_DESC destinationDescription{};
    if (FAILED(texture->GetLevelDesc(0, &destinationDescription)) ||
        destinationDescription.Width < static_cast<UINT>(width) ||
        destinationDescription.Height < static_cast<UINT>(height) ||
        destinationDescription.Format != D3DFMT_A8R8G8B8)
    {
        source->Release();
        return false;
    }

    D3DLOCKED_RECT sourceLocked{};
    D3DLOCKED_RECT destinationLocked{};
    if (FAILED(source->LockRect(&sourceLocked, nullptr, D3DLOCK_READONLY)) ||
        FAILED(texture->LockRect(0, &destinationLocked, nullptr, 0)))
    {
        if (sourceLocked.pBits != nullptr)
            source->UnlockRect();
        source->Release();
        return false;
    }

    const int top = static_cast<int>(description.Height) - y - height;
    bool success = true;
    for (int row = 0; row < height && success; ++row)
    {
        // glCopyTexSubImage2D maps the framebuffer's bottom source row to v=0.
        // D3D textures sample v=0 from their top row, so store source rows in
        // reverse D3D surface order to preserve the existing RenderAPI UVs.
        const int sourceY = top + (height - row - 1);
        const unsigned char* sourceRow = static_cast<const unsigned char*>(sourceLocked.pBits) +
                                         static_cast<std::size_t>(sourceY) * static_cast<std::size_t>(sourceLocked.Pitch);
        unsigned char* destinationRow = static_cast<unsigned char*>(destinationLocked.pBits) +
                                        static_cast<std::size_t>(row) * static_cast<std::size_t>(destinationLocked.Pitch);
        for (int column = 0; column < width; ++column)
        {
            SurfacePixel pixel{};
            if (!decodeSurfacePixel(sourceRow, x + column, description.Format, &pixel))
            {
                success = false;
                break;
            }
            writeA8R8G8B8(destinationRow + static_cast<std::size_t>(column) * 4u, pixel);
        }
    }

    texture->UnlockRect(0);
    source->UnlockRect();
    source->Release();
    return success;
}

void renderSetLegacyPresentationGamma(bool)
{
}
}

#endif
