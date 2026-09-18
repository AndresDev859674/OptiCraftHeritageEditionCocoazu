#pragma once

#include <string>

#include "platform/PlatformConfig.h"

#if PLATFORM_PC

enum class PcRenderBackendType
{
    OpenGL = 0,
    Direct3D9 = 1
};

PcRenderBackendType pcRenderBackendFromString(const std::string& value);
const char* pcRenderBackendConfigName(PcRenderBackendType backend);
const char* pcRenderBackendDisplayName(PcRenderBackendType backend);
bool pcRenderBackendIsSupported(PcRenderBackendType backend);

void pcRenderBackendSetRequested(PcRenderBackendType backend);
PcRenderBackendType pcRenderBackendGetRequested();

void pcRenderBackendSetActive(PcRenderBackendType backend);
PcRenderBackendType pcRenderBackendGetActive();
bool pcRenderBackendRestartRequired();
bool pcRenderBackendIsDirect3D9();

#endif
