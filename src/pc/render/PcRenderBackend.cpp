#include "pc/render/PcRenderBackend.h"

#if PLATFORM_PC

#include <algorithm>
#include <cctype>

namespace
{
PcRenderBackendType g_requestedBackend = PcRenderBackendType::OpenGL;
PcRenderBackendType g_activeBackend = PcRenderBackendType::OpenGL;

std::string normalizeBackendName(std::string value)
{
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
        return std::isspace(c) != 0 || c == '-' || c == '_';
    }), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}
}

PcRenderBackendType pcRenderBackendFromString(const std::string& value)
{
    const std::string normalized = normalizeBackendName(value);
    if (normalized == "d3d9" || normalized == "direct3d9" || normalized == "directx9")
        return PcRenderBackendType::Direct3D9;
    return PcRenderBackendType::OpenGL;
}

const char* pcRenderBackendConfigName(PcRenderBackendType backend)
{
    return backend == PcRenderBackendType::Direct3D9 ? "d3d9" : "opengl";
}

const char* pcRenderBackendDisplayName(PcRenderBackendType backend)
{
    return backend == PcRenderBackendType::Direct3D9 ? "Direct3D 9" : "OpenGL";
}

bool pcRenderBackendIsSupported(PcRenderBackendType backend)
{
    if (backend == PcRenderBackendType::OpenGL)
        return true;
#if PLATFORM_PC_LEGACY && defined(MC_WIN32)
    return backend == PcRenderBackendType::Direct3D9;
#else
    return false;
#endif
}

void pcRenderBackendSetRequested(PcRenderBackendType backend)
{
    g_requestedBackend = pcRenderBackendIsSupported(backend) ? backend : PcRenderBackendType::OpenGL;
}

PcRenderBackendType pcRenderBackendGetRequested()
{
    return g_requestedBackend;
}

void pcRenderBackendSetActive(PcRenderBackendType backend)
{
    g_activeBackend = backend;
}

PcRenderBackendType pcRenderBackendGetActive()
{
    return g_activeBackend;
}

bool pcRenderBackendRestartRequired()
{
    return g_requestedBackend != g_activeBackend;
}

bool pcRenderBackendIsDirect3D9()
{
    return g_activeBackend == PcRenderBackendType::Direct3D9;
}

#endif
