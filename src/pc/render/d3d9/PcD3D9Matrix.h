#pragma once

#include "platform/PlatformConfig.h"

#if PLATFORM_PC && defined(MC_WIN32)

#include <array>
#include <d3d9.h>

struct PcD3D9Matrix
{
    std::array<float, 16> values{};
};

PcD3D9Matrix pcD3D9MatrixIdentity();
PcD3D9Matrix pcD3D9MatrixMultiply(const PcD3D9Matrix& left, const PcD3D9Matrix& right);
PcD3D9Matrix pcD3D9MatrixMultiplyAffine(const PcD3D9Matrix& left, const PcD3D9Matrix& right);
PcD3D9Matrix pcD3D9MatrixTranslation(float x, float y, float z);
PcD3D9Matrix pcD3D9MatrixScale(float x, float y, float z);
PcD3D9Matrix pcD3D9MatrixRotation(float degrees, float x, float y, float z);
PcD3D9Matrix pcD3D9MatrixFrustum(double left, double right, double bottom, double top, double nearValue, double farValue);
PcD3D9Matrix pcD3D9MatrixOrtho(double left, double right, double bottom, double top, double nearValue, double farValue);
D3DMATRIX pcD3D9MatrixToD3D(const PcD3D9Matrix& matrix);
D3DMATRIX pcD3D9TextureMatrixToD3D2D(const PcD3D9Matrix& matrix);
D3DMATRIX pcD3D9ProjectionToD3D(const PcD3D9Matrix& projection);

#endif
