#include "pc/render/d3d9/PcD3D9Context.h"

#if PLATFORM_PC && defined(MC_WIN32)

#include <algorithm>
#include <array>

#include <d3d9.h>
#include <SDL.h>
#include <SDL_syswm.h>

#include "pc/render/d3d9/PcD3D9Internal.h"
#include "platform/Log.h"

namespace
{
SDL_Window* g_window = nullptr;
IDirect3D9* g_d3d = nullptr;
IDirect3DDevice9* g_device = nullptr;
D3DPRESENT_PARAMETERS g_present{};
bool g_sceneActive = false;
bool g_resizePending = false;
int g_samples = 0;
int g_backBufferWidth = 1;
int g_backBufferHeight = 1;

HWND getWindowHandle(SDL_Window* window)
{
    if (window == nullptr)
        return nullptr;

    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info) != SDL_TRUE)
        return nullptr;
    return info.info.win.window;
}

D3DFORMAT chooseDepthFormat(D3DFORMAT adapterFormat, D3DFORMAT backBufferFormat)
{
    constexpr std::array<D3DFORMAT, 3> candidates = {
        D3DFMT_D24S8,
        D3DFMT_D24X8,
        D3DFMT_D16
    };

    for (D3DFORMAT format : candidates)
    {
        if (FAILED(g_d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                            adapterFormat, D3DUSAGE_DEPTHSTENCIL,
                                            D3DRTYPE_SURFACE, format)))
            continue;
        if (FAILED(g_d3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                                 adapterFormat, backBufferFormat, format)))
            continue;
        return format;
    }
    return D3DFMT_D16;
}

D3DMULTISAMPLE_TYPE sampleTypeForCount(int samples)
{
    switch (samples)
    {
        case 2: return D3DMULTISAMPLE_2_SAMPLES;
        case 4: return D3DMULTISAMPLE_4_SAMPLES;
        case 8: return D3DMULTISAMPLE_8_SAMPLES;
        case 16: return D3DMULTISAMPLE_16_SAMPLES;
        default: return D3DMULTISAMPLE_NONE;
    }
}

int chooseSampleCount(int requested, D3DFORMAT adapterFormat, D3DFORMAT depthFormat)
{
    constexpr std::array<int, 5> candidates = {16, 8, 4, 2, 0};
    for (int candidate : candidates)
    {
        if (candidate > requested)
            continue;
        const D3DMULTISAMPLE_TYPE type = sampleTypeForCount(candidate);
        if (type == D3DMULTISAMPLE_NONE)
            return 0;

        DWORD colorQuality = 0;
        DWORD depthQuality = 0;
        if (FAILED(g_d3d->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                                     adapterFormat, TRUE, type, &colorQuality)))
            continue;
        if (FAILED(g_d3d->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                                     depthFormat, TRUE, type, &depthQuality)))
            continue;
        return candidate;
    }
    return 0;
}

void updateBackBufferSize()
{
    int width = 1;
    int height = 1;
    if (g_window != nullptr)
        SDL_GetWindowSize(g_window, &width, &height);
    g_present.BackBufferWidth = static_cast<UINT>(std::max(width, 1));
    g_present.BackBufferHeight = static_cast<UINT>(std::max(height, 1));
}

bool refreshBackBufferSize()
{
    if (g_device == nullptr)
        return false;

    IDirect3DSurface9* backBuffer = nullptr;
    if (FAILED(g_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) || backBuffer == nullptr)
        return false;

    D3DSURFACE_DESC description{};
    const HRESULT result = backBuffer->GetDesc(&description);
    backBuffer->Release();
    if (FAILED(result))
        return false;

    g_backBufferWidth = std::max(static_cast<int>(description.Width), 1);
    g_backBufferHeight = std::max(static_cast<int>(description.Height), 1);

    D3DVIEWPORT9 viewport{};
    viewport.X = 0;
    viewport.Y = 0;
    viewport.Width = static_cast<DWORD>(g_backBufferWidth);
    viewport.Height = static_cast<DWORD>(g_backBufferHeight);
    viewport.MinZ = 0.0f;
    viewport.MaxZ = 1.0f;
    if (FAILED(g_device->SetViewport(&viewport)))
        return false;

    pcD3D9State().viewport = {{0, 0, g_backBufferWidth, g_backBufferHeight}};
    return true;
}

bool beginScene()
{
    if (g_device == nullptr)
        return false;
    if (g_sceneActive)
        return true;
    if (FAILED(g_device->BeginScene()))
        return false;
    g_sceneActive = true;
    return true;
}

void endScene()
{
    if (g_device == nullptr || !g_sceneActive)
        return;
    g_device->EndScene();
    g_sceneActive = false;
}

bool resetDevice()
{
    if (g_device == nullptr)
        return false;

    endScene();
    pcD3D9CaptureStateForReset();
    updateBackBufferSize();
    pcD3D9ClearQueries();
    pcD3D9ReleaseDynamicMeshBuffer();
    if (FAILED(g_device->Reset(&g_present)))
        return false;
    if (!refreshBackBufferSize())
        return false;
    g_resizePending = false;
    if (!beginScene())
        return false;
    pcD3D9RestoreStateAfterReset();
    return true;
}
}

bool pcD3D9Initialize(SDL_Window* window, int requestedSamples)
{
    pcD3D9Shutdown();
    g_window = window;

    const HWND hwnd = getWindowHandle(window);
    if (hwnd == nullptr)
        return false;

    g_d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (g_d3d == nullptr)
        return false;

    D3DDISPLAYMODE displayMode{};
    if (FAILED(g_d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &displayMode)))
    {
        pcD3D9Shutdown();
        return false;
    }

    g_present = {};
    g_present.Windowed = TRUE;
    g_present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_present.BackBufferFormat = D3DFMT_UNKNOWN;
    g_present.EnableAutoDepthStencil = TRUE;
    g_present.AutoDepthStencilFormat = chooseDepthFormat(displayMode.Format, displayMode.Format);
    g_present.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    updateBackBufferSize();

    g_samples = chooseSampleCount(requestedSamples, displayMode.Format, g_present.AutoDepthStencilFormat);
    g_present.MultiSampleType = sampleTypeForCount(g_samples);
    g_present.MultiSampleQuality = 0;

    D3DCAPS9 caps{};
    g_d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps);
    const bool hardwareTransform = (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0;
    const unsigned int pixelShaderMajor = (caps.PixelShaderVersion >> 8) & 0xffu;
    const unsigned int pixelShaderMinor = caps.PixelShaderVersion & 0xffu;
    const unsigned int vertexShaderMajor = (caps.VertexShaderVersion >> 8) & 0xffu;
    const unsigned int vertexShaderMinor = caps.VertexShaderVersion & 0xffu;
    MC_LOG_INFO("d3d9",
                "Fixed-function renderer: HW_TL=%s textureStages=%lu simultaneousTextures=%lu "
                "PixelShaderVersion=%u.%u VertexShaderVersion=%u.%u (programmable shaders disabled)\n",
                hardwareTransform ? "yes" : "no",
                static_cast<unsigned long>(caps.MaxTextureBlendStages),
                static_cast<unsigned long>(caps.MaxSimultaneousTextures),
                pixelShaderMajor, pixelShaderMinor, vertexShaderMajor, vertexShaderMinor);
    const DWORD commonFlags = D3DCREATE_FPU_PRESERVE;
    const std::array<DWORD, 3> behaviorCandidates = {
        commonFlags | (hardwareTransform ? D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING),
        commonFlags | D3DCREATE_MIXED_VERTEXPROCESSING,
        commonFlags | D3DCREATE_SOFTWARE_VERTEXPROCESSING
    };

    HRESULT result = E_FAIL;
    for (DWORD behavior : behaviorCandidates)
    {
        result = g_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, behavior,
                                     &g_present, &g_device);
        if (SUCCEEDED(result))
            break;
    }

    if (FAILED(result) || g_device == nullptr)
    {
        pcD3D9Shutdown();
        return false;
    }

    if (!refreshBackBufferSize())
    {
        pcD3D9Shutdown();
        return false;
    }
    if (!beginScene())
    {
        pcD3D9Shutdown();
        return false;
    }
    pcD3D9RestoreDefaultState();
    return true;
}

void pcD3D9Shutdown()
{
    endScene();
    pcD3D9ClearQueries();
    pcD3D9ClearDisplayLists();
    pcD3D9ClearTextures();
    pcD3D9ReleaseDynamicMeshBuffer();
    pcD3D9ReleaseSharedMeshResources();
    if (g_device != nullptr)
    {
        g_device->Release();
        g_device = nullptr;
    }
    if (g_d3d != nullptr)
    {
        g_d3d->Release();
        g_d3d = nullptr;
    }
    g_window = nullptr;
    g_present = {};
    g_resizePending = false;
    g_samples = 0;
    g_backBufferWidth = 1;
    g_backBufferHeight = 1;
}

bool pcD3D9Present()
{
    if (g_device == nullptr)
        return false;

    endScene();
    const HRESULT result = g_device->Present(nullptr, nullptr, nullptr, nullptr);
    if (result == D3DERR_DEVICELOST)
    {
        const HRESULT cooperative = g_device->TestCooperativeLevel();
        if (cooperative == D3DERR_DEVICENOTRESET)
            resetDevice();
        return false;
    }
    if (FAILED(result))
        return false;

    if (g_resizePending)
    {
        if (!pcD3D9ApplyPendingResize())
            return false;
        if (g_sceneActive)
            return true;
    }
    return beginScene();
}

void pcD3D9RequestResize()
{
    g_resizePending = true;
}

bool pcD3D9ApplyPendingResize()
{
    if (!g_resizePending)
        return true;
    if (g_device == nullptr)
        return false;

    int width = 1;
    int height = 1;
    if (g_window != nullptr)
        SDL_GetWindowSize(g_window, &width, &height);
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (width == g_backBufferWidth && height == g_backBufferHeight)
    {
        g_resizePending = false;
        return true;
    }
    return resetDevice();
}

bool pcD3D9GetBackBufferSize(int* width, int* height)
{
    if (width != nullptr)
        *width = g_backBufferWidth;
    if (height != nullptr)
        *height = g_backBufferHeight;
    return g_device != nullptr && g_backBufferWidth > 0 && g_backBufferHeight > 0;
}

bool pcD3D9SuspendScene()
{
    if (g_device == nullptr)
        return false;
    if (!g_sceneActive)
        return true;
    const HRESULT result = g_device->EndScene();
    if (FAILED(result))
        return false;
    g_sceneActive = false;
    return true;
}

bool pcD3D9ResumeScene()
{
    return beginScene();
}

bool pcD3D9SceneActive()
{
    return g_sceneActive;
}

IDirect3DDevice9* pcD3D9GetDevice()
{
    return g_device;
}

int pcD3D9GetSamples()
{
    return g_samples;
}

#endif
