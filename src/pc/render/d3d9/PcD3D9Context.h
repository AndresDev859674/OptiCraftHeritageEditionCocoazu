#pragma once

#include "platform/PlatformConfig.h"

#if PLATFORM_PC && defined(MC_WIN32)

struct SDL_Window;
struct IDirect3D9;
struct IDirect3DDevice9;

bool pcD3D9Initialize(SDL_Window* window, int requestedSamples);
void pcD3D9Shutdown();
bool pcD3D9Present();
void pcD3D9RequestResize();
bool pcD3D9ApplyPendingResize();
bool pcD3D9GetBackBufferSize(int* width, int* height);
bool pcD3D9SuspendScene();
bool pcD3D9ResumeScene();
bool pcD3D9SceneActive();
IDirect3DDevice9* pcD3D9GetDevice();
int pcD3D9GetSamples();

#endif
