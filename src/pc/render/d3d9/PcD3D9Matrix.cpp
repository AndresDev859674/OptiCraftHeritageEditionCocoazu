#include "pc/render/d3d9/PcD3D9Matrix.h"

#if PLATFORM_PC && defined(MC_WIN32)

#include <cmath>
#include <d3d9.h>

PcD3D9Matrix pcD3D9MatrixIdentity()
{
    PcD3D9Matrix matrix{};
    matrix.values[0] = 1.0f;
    matrix.values[5] = 1.0f;
    matrix.values[10] = 1.0f;
    matrix.values[15] = 1.0f;
    return matrix;
}

PcD3D9Matrix pcD3D9MatrixMultiply(const PcD3D9Matrix& left, const PcD3D9Matrix& right)
{
    PcD3D9Matrix result{};
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            float value = 0.0f;
            for (int k = 0; k < 4; ++k)
                value += left.values[k * 4 + row] * right.values[column * 4 + k];
            result.values[column * 4 + row] = value;
        }
    }
    return result;
}

PcD3D9Matrix pcD3D9MatrixMultiplyAffine(const PcD3D9Matrix& left, const PcD3D9Matrix& right)
{
    PcD3D9Matrix result{};
    for (int column = 0; column < 3; ++column)
    {
        for (int row = 0; row < 3; ++row)
        {
            result.values[column * 4 + row] =
                left.values[row] * right.values[column * 4] +
                left.values[4 + row] * right.values[column * 4 + 1] +
                left.values[8 + row] * right.values[column * 4 + 2];
        }
    }

    for (int row = 0; row < 3; ++row)
    {
        result.values[12 + row] =
            left.values[row] * right.values[12] +
            left.values[4 + row] * right.values[13] +
            left.values[8 + row] * right.values[14] +
            left.values[12 + row];
    }

    result.values[15] = 1.0f;
    return result;
}

PcD3D9Matrix pcD3D9MatrixTranslation(float x, float y, float z)
{
    PcD3D9Matrix matrix = pcD3D9MatrixIdentity();
    matrix.values[12] = x;
    matrix.values[13] = y;
    matrix.values[14] = z;
    return matrix;
}

PcD3D9Matrix pcD3D9MatrixScale(float x, float y, float z)
{
    PcD3D9Matrix matrix = pcD3D9MatrixIdentity();
    matrix.values[0] = x;
    matrix.values[5] = y;
    matrix.values[10] = z;
    return matrix;
}

PcD3D9Matrix pcD3D9MatrixRotation(float degrees, float x, float y, float z)
{
    const float length = std::sqrt(x * x + y * y + z * z);
    if (length <= 0.000001f)
        return pcD3D9MatrixIdentity();

    x /= length;
    y /= length;
    z /= length;
    const float radians = degrees * 0.01745329251994329577f;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    const float oneMinusC = 1.0f - c;

    PcD3D9Matrix matrix = pcD3D9MatrixIdentity();
    matrix.values[0] = x * x * oneMinusC + c;
    matrix.values[4] = x * y * oneMinusC - z * s;
    matrix.values[8] = x * z * oneMinusC + y * s;

    matrix.values[1] = y * x * oneMinusC + z * s;
    matrix.values[5] = y * y * oneMinusC + c;
    matrix.values[9] = y * z * oneMinusC - x * s;

    matrix.values[2] = z * x * oneMinusC - y * s;
    matrix.values[6] = z * y * oneMinusC + x * s;
    matrix.values[10] = z * z * oneMinusC + c;
    return matrix;
}

PcD3D9Matrix pcD3D9MatrixFrustum(double left, double right, double bottom, double top, double nearValue, double farValue)
{
    PcD3D9Matrix matrix{};
    matrix.values[0] = static_cast<float>((2.0 * nearValue) / (right - left));
    matrix.values[5] = static_cast<float>((2.0 * nearValue) / (top - bottom));
    matrix.values[8] = static_cast<float>((right + left) / (right - left));
    matrix.values[9] = static_cast<float>((top + bottom) / (top - bottom));
    matrix.values[10] = static_cast<float>(-(farValue + nearValue) / (farValue - nearValue));
    matrix.values[11] = -1.0f;
    matrix.values[14] = static_cast<float>(-(2.0 * farValue * nearValue) / (farValue - nearValue));
    return matrix;
}

PcD3D9Matrix pcD3D9MatrixOrtho(double left, double right, double bottom, double top, double nearValue, double farValue)
{
    PcD3D9Matrix matrix = pcD3D9MatrixIdentity();
    matrix.values[0] = static_cast<float>(2.0 / (right - left));
    matrix.values[5] = static_cast<float>(2.0 / (top - bottom));
    matrix.values[10] = static_cast<float>(-2.0 / (farValue - nearValue));
    matrix.values[12] = static_cast<float>(-(right + left) / (right - left));
    matrix.values[13] = static_cast<float>(-(top + bottom) / (top - bottom));
    matrix.values[14] = static_cast<float>(-(farValue + nearValue) / (farValue - nearValue));
    return matrix;
}

D3DMATRIX pcD3D9MatrixToD3D(const PcD3D9Matrix& matrix)
{
    D3DMATRIX result{};
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            result.m[row][column] = matrix.values[row * 4 + column];
    return result;
}

D3DMATRIX pcD3D9TextureMatrixToD3D2D(const PcD3D9Matrix& matrix)
{
    // D3DTTFF_COUNT2 treats texture coordinates as a 3-component row vector
    // (u, v, 1). Translation therefore lives in _31/_32 instead of the
    // _41/_42 slots used by ordinary 4D geometry transforms.
    D3DMATRIX result{};
    result._11 = matrix.values[0];
    result._12 = matrix.values[1];
    result._21 = matrix.values[4];
    result._22 = matrix.values[5];
    result._31 = matrix.values[12];
    result._32 = matrix.values[13];
    result._33 = 1.0f;
    result._44 = 1.0f;
    return result;
}

D3DMATRIX pcD3D9ProjectionToD3D(const PcD3D9Matrix& projection)
{
    PcD3D9Matrix correction = pcD3D9MatrixIdentity();
    correction.values[10] = 0.5f;
    correction.values[14] = 0.5f;
    return pcD3D9MatrixToD3D(pcD3D9MatrixMultiply(correction, projection));
}

#endif
