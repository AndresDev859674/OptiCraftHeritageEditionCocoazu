#include "platform/Log.h"
#include "wii/render/WiiNativeTexture.h"

#if defined(WII_PLATFORM)
#include "wii/render/WiiNativeStateSnapshot.h"
#include "wii/render/WiiRenderTypes.h"
#include "wii/WiiEarlyInit.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <malloc.h>
#include <unordered_map>

namespace
{
    struct NativeTexture
    {
        GXTexObj object;
        void *pixels = nullptr;
        uint32_t bytes = 0;   // allocation behind pixels, every level included
        int width = 0;
        int height = 0;
        int maxLevel = 0;
        uint8_t format = GX_TF_RGB5A3;
        bool linear = false;
        bool mipmaps = false;
        bool clamp = false;
        bool mipmapLinear = true;
        int anisotropy = 1;
    };

    std::unordered_map<int, NativeTexture> s_textures;
    int s_currentTexture = -1;
    unsigned int s_nextTextureId = 1;
    bool s_texelCacheDirty = false;

    uint16_t packRgb5A3(const unsigned char *rgba)
    {
        const uint8_t red = rgba[0];
        const uint8_t green = rgba[1];
        const uint8_t blue = rgba[2];
        const uint8_t alpha = rgba[3];

        if (alpha >= 237) {
            return static_cast<uint16_t>(0x8000 |
                ((red & 0xf8) << 7) |
                ((green & 0xf8) << 2) |
                ((blue & 0xf8) >> 3));
        }

        return static_cast<uint16_t>(
            ((alpha & 0xe0) << 7) |
            ((red & 0xf0) << 4) |
            (green & 0xf0) |
            ((blue & 0xf0) >> 4));
    }

    void uploadRgb5A3(const unsigned char *src, int width, int height, void *dst)
    {
        const int blocksWide = (width + 3) / 4;
        unsigned char *dstBytes = static_cast<unsigned char *>(dst);

        for (int blockY = 0; blockY < height; blockY += 4) {
            const int blockRow = blockY / 4;
            for (int blockX = 0; blockX < width; blockX += 4) {
                const int blockColumn = blockX / 4;
                unsigned char *block = dstBytes +
                    (blockRow * blocksWide + blockColumn) * 32;

                const int rows = height - blockY < 4 ? height - blockY : 4;
                const int columns = width - blockX < 4 ? width - blockX : 4;
                for (int y = 0; y < rows; ++y) {
                    uint16_t *dstRow = reinterpret_cast<uint16_t *>(block + y * 8);
                    const unsigned char *srcRow = src +
                        ((blockY + y) * width + blockX) * 4;
                    for (int x = 0; x < columns; ++x)
                        dstRow[x] = packRgb5A3(srcRow + x * 4);
                }
            }
        }
    }

    void uploadRgb5A3SubRect(const unsigned char *src, int srcWidth, int srcHeight,
                               int dstWidth, int dstHeight, int dstX, int dstY, void *dst)
    {
        unsigned char *dstBytes = static_cast<unsigned char *>(dst);
        const int blocksWide = (dstWidth + 3) / 4;
        for (int y = 0; y < srcHeight; ++y) {
            const int py = dstY + y;
            if (py < 0 || py >= dstHeight)
                continue;
            for (int x = 0; x < srcWidth; ++x) {
                const int px = dstX + x;
                if (px < 0 || px >= dstWidth)
                    continue;
                const int blockX = px >> 2;
                const int blockY = py >> 2;
                const int inBlockX = px & 3;
                const int inBlockY = py & 3;
                uint16_t *dstPixel = reinterpret_cast<uint16_t *>(
                    dstBytes + (blockY * blocksWide + blockX) * 32 +
                    (inBlockY * 4 + inBlockX) * 2);
                *dstPixel = packRgb5A3(src + (y * srcWidth + x) * 4);
            }
        }
    }

    void uploadRgba8(const unsigned char *src, int width, int height, void *dst)
    {
        const int blocksWide = (width + 3) / 4;
        unsigned char *dstBytes = static_cast<unsigned char *>(dst);

        for (int blockY = 0; blockY < height; blockY += 4) {
            const int blockRow = blockY / 4;
            for (int blockX = 0; blockX < width; blockX += 4) {
                const int blockColumn = blockX / 4;
                unsigned char *block = dstBytes +
                    (blockRow * blocksWide + blockColumn) * 64;
                const int rows = height - blockY < 4 ? height - blockY : 4;
                const int columns = width - blockX < 4 ? width - blockX : 4;

                for (int y = 0; y < rows; ++y) {
                    for (int x = 0; x < columns; ++x) {
                        const int pixel = y * 4 + x;
                        const unsigned char *rgba = src +
                            ((blockY + y) * width + blockX + x) * 4;
                        block[pixel * 2] = rgba[3];
                        block[pixel * 2 + 1] = rgba[0];
                        block[32 + pixel * 2] = rgba[1];
                        block[32 + pixel * 2 + 1] = rgba[2];
                    }
                }
            }
        }
    }

    void uploadRgba8SubRect(const unsigned char *src, int srcWidth, int srcHeight,
                            int dstWidth, int dstHeight, int dstX, int dstY, void *dst)
    {
        unsigned char *dstBytes = static_cast<unsigned char *>(dst);
        const int blocksWide = (dstWidth + 3) / 4;
        for (int y = 0; y < srcHeight; ++y) {
            const int py = dstY + y;
            if (py < 0 || py >= dstHeight)
                continue;
            for (int x = 0; x < srcWidth; ++x) {
                const int px = dstX + x;
                if (px < 0 || px >= dstWidth)
                    continue;
                const int blockX = px >> 2;
                const int blockY = py >> 2;
                const int pixel = (py & 3) * 4 + (px & 3);
                unsigned char *block = dstBytes +
                    (blockY * blocksWide + blockX) * 64;
                const unsigned char *rgba = src + (y * srcWidth + x) * 4;
                block[pixel * 2] = rgba[3];
                block[pixel * 2 + 1] = rgba[0];
                block[32 + pixel * 2] = rgba[1];
                block[32 + pixel * 2 + 1] = rgba[2];
            }
        }
    }

    uint32_t mipLevelOffset(const NativeTexture &texture, int level)
    {
        uint32_t offset = 0;
        int width = texture.width;
        int height = texture.height;
        for (int currentLevel = 0; currentLevel < level; ++currentLevel)
        {
            offset += GX_GetTexBufferSize((u16)width, (u16)height, texture.format, GX_FALSE, 0);
            width = width > 1 ? width >> 1 : 1;
            height = height > 1 ? height >> 1 : 1;
        }
        return offset;
    }

    void releaseTexture(NativeTexture &texture)
    {
        if (texture.pixels != nullptr)
            free(texture.pixels);
        texture.pixels = nullptr;
        texture.bytes = 0;
    }

    void markTexelCacheDirty()
    {
        s_texelCacheDirty = true;
    }

    void flushTexelCacheIfDirty()
    {
        if (!s_texelCacheDirty)
            return;
        GX_InvalidateTexAll();
        s_texelCacheDirty = false;
    }

    void refreshSamplerObject(NativeTexture &texture)
    {
        const bool useMipmaps = texture.mipmaps && texture.maxLevel > 0;
        // Same rule as the creation path: GX_REPEAT masks the coordinate and is
        // only correct on power-of-two dimensions. This runs right after every
        // upload, because RenderEngine applies its sampling policy once the
        // backend object exists -- so without the check here it would put
        // GX_REPEAT straight back on the textures that cannot take it.
        const bool isPowerOfTwo = (texture.width & (texture.width - 1)) == 0 &&
                                  (texture.height & (texture.height - 1)) == 0;
        const u8 wrap = (texture.clamp || !isPowerOfTwo) ? GX_CLAMP : GX_REPEAT;
        GX_InitTexObj(&texture.object, texture.pixels, (u16)texture.width, (u16)texture.height,
                      texture.format, wrap, wrap, useMipmaps ? GX_TRUE : GX_FALSE);
        const u8 minFilter = texture.linear
            ? (useMipmaps ? GX_LIN_MIP_LIN : GX_LINEAR)
            : (useMipmaps ? (texture.mipmapLinear ? GX_NEAR_MIP_LIN : GX_NEAR_MIP_NEAR) : GX_NEAR);
        const u8 magFilter = texture.linear ? GX_LINEAR : GX_NEAR;
        const u8 maxAniso = texture.anisotropy >= 4
            ? GX_ANISO_4
            : (texture.anisotropy >= 2 ? GX_ANISO_2 : GX_ANISO_1);
        GX_InitTexObjLOD(&texture.object, minFilter, magFilter, 0.0f,
                         useMipmaps ? (f32)texture.maxLevel : 0.0f,
                         0.0f, GX_ENABLE, GX_ENABLE, maxAniso);
    }
}
#endif

void wii_native_texture_generate_names(int count, int *textures)
{
#if defined(WII_PLATFORM)
    if (count <= 0 || textures == nullptr)
        return;
    for (int i = 0; i < count; ++i)
        textures[i] = static_cast<int>(s_nextTextureId++);
#else
    (void)count;
    (void)textures;
#endif
}

void wii_native_texture_delete_names(int count, const int *textures)
{
#if defined(WII_PLATFORM)
    if (count <= 0 || textures == nullptr)
        return;
    for (int i = 0; i < count; ++i)
        wii_native_texture_remove(textures[i]);
#else
    (void)count;
    (void)textures;
#endif
}

bool wii_native_texture_refresh(int texture)
{
#if defined(WII_PLATFORM)
    return texture >= 0 && s_textures.find(texture) != s_textures.end();
#else
    (void)texture;
    return false;
#endif
}

bool wii_native_texture_begin_upload(int texture, int width, int height, int maxLevel,
                                     bool linear, bool clamp, bool highPrecision)
{
#if defined(WII_PLATFORM)
    if (texture < 0 || width <= 0 || height <= 0 || maxLevel < 0 ||
        width > static_cast<int>(std::numeric_limits<u16>::max()) ||
        height > static_cast<int>(std::numeric_limits<u16>::max()) ||
        maxLevel > static_cast<int>(std::numeric_limits<u8>::max()) ||
        maxLevel >= std::numeric_limits<int>::digits)
    {
#if MC_LOG_LEVEL >= 2
        MC_LOG_INFO("wii", "[WII][GX][TEXFAIL] begin id=%d size=%dx%d max=%d invalid\n",
               texture, width, height, maxLevel);
#endif
        return false;
    }

    // GX addresses texture dimensions in a 10-bit field, so 1024 is a hardware
    // ceiling, not a budget. The u16 bounds above are far too generous to catch
    // it: GX_InitTexObj() silently keeps the low bits, so a 1230-wide texture is
    // sampled as 1230 & 1023 = 206 wide out of a buffer tiled for 1230, and the
    // image comes out repeated across itself rather than simply wrong. That is
    // what data/assets/legacy/panorama.png (1230x216) looked like on hardware:
    // garbage that reads as a rendering bug rather than an asset that cannot fit.
    //
    // Rejecting is deliberate. Cropping to 1024 here would not work -- the
    // tiling stride is derived from the full width, so GX would still read rows
    // that do not line up -- and silently resampling art at load time is not
    // this layer's call to make. Fail loudly and name the texture instead.
    static const int GX_MAX_TEXTURE_DIMENSION = 1024;
    if (width > GX_MAX_TEXTURE_DIMENSION || height > GX_MAX_TEXTURE_DIMENSION)
    {
        MC_LOG_ERROR("wii", "[WII][GX][TEXFAIL] begin id=%d size=%dx%d exceeds the %d texel GX limit; resize the asset\n",
               texture, width, height, GX_MAX_TEXTURE_DIMENSION);
        return false;
    }

    const uint8_t format = highPrecision ? GX_TF_RGBA8 : GX_TF_RGB5A3;
    // libogc's maxlod here is a LEVEL COUNT, not the highest level index: the
    // mipmap branch loads it into CTR and sums exactly that many levels from
    // level 0 (GX_GetTexBufferSize disassembly, libogc.a). Passing maxLevel
    // sized the buffer for levels 0..maxLevel-1 and the last level's upload
    // wrote past the allocation -- a DSI in _free_r during the first
    // setupTexture() once the Wii profile defaulted ofMipmapLevel to 2.
    // GX_InitTexObjLOD's maxlod below is the index, as its name says.
    const uint32_t size = GX_GetTexBufferSize((u16)width, (u16)height, format,
                                              maxLevel > 0 ? GX_TRUE : GX_FALSE,
                                              (u8)(maxLevel + 1));
    void *pixels = memalign(32, size);
    if (pixels == nullptr)
    {
        MC_LOG_INFO("wii", "[WII][GX][TEXFAIL] alloc id=%d size=%dx%d bytes=%u max=%d\n",
               texture, width, height, (unsigned int)size, maxLevel);
        return false;
    }
    std::memset(pixels, 0, size);

    NativeTexture &native = s_textures[texture];
    releaseTexture(native);
    native.pixels = pixels;
    native.bytes = size;
    native.width = width;
    native.height = height;
    native.maxLevel = maxLevel;
    native.format = format;
    native.linear = linear;
    native.mipmaps = maxLevel > 0;
    native.clamp = clamp;
    native.mipmapLinear = true;
    native.anisotropy = 1;

    // GX_REPEAT wraps by masking the coordinate, which only produces the right
    // texel for a power-of-two dimension: on anything else the mask belongs to
    // the next power of two and the texture is sampled as if it were that wide,
    // repeating and shearing across itself. Every UI texture that renders
    // correctly here is 256, 128 or 16 square; the ones that came out as
    // repeated garbage -- the 640x480 startup logos, the 995x205 legacy title --
    // are exactly the ones that are not.
    //
    // Clamping instead is safe for them: a texture that cannot be repeated
    // correctly has nothing to lose by not repeating, and these are drawn as
    // single quads with 0..1 coordinates that never leave the texture anyway.
    const bool isPowerOfTwo = (width & (width - 1)) == 0 && (height & (height - 1)) == 0;
    const u8 wrap = (clamp || !isPowerOfTwo) ? GX_CLAMP : GX_REPEAT;
    GX_InitTexObj(&native.object, pixels, (u16)width, (u16)height, format,
                  wrap, wrap, maxLevel > 0 ? GX_TRUE : GX_FALSE);
    const u8 minFilter = linear ? GX_LINEAR : (maxLevel > 0 ? GX_NEAR_MIP_LIN : GX_NEAR);
    const u8 magFilter = linear ? GX_LINEAR : GX_NEAR;
    GX_InitTexObjLOD(&native.object, minFilter, magFilter, 0.0f, (f32)maxLevel,
                     0.0f, GX_ENABLE, GX_ENABLE, GX_ANISO_1);
    DCFlushRange(pixels, size);
    markTexelCacheDirty();
#if MC_LOG_LEVEL >= 2
    {
        static unsigned int loggedTextures = 0;
        if (loggedTextures < 32)
        {
            MC_LOG_INFO("wii", "[WII][GX][TEX] n=%u id=%d size=%dx%d bytes=%u max=%d linear=%d clamp=%d ptr=%p\n",
                   loggedTextures, texture, width, height, (unsigned int)size, maxLevel,
                   linear ? 1 : 0, clamp ? 1 : 0, pixels);
            ++loggedTextures;
        }
    }
#endif
    return true;
#else
    (void)texture; (void)width; (void)height; (void)maxLevel; (void)linear; (void)clamp; (void)highPrecision;
    return false;
#endif
}

bool wii_native_texture_set_parameters(int texture, bool linear, bool mipmaps, bool clamp)
{
#if defined(WII_PLATFORM)
    auto it = s_textures.find(texture);
    if (it == s_textures.end() || it->second.pixels == nullptr)
        return false;

    NativeTexture &native = it->second;
    const bool useMipmaps = mipmaps && native.maxLevel > 0;
    if (native.linear == linear && native.mipmaps == useMipmaps && native.clamp == clamp)
        return true;

    native.linear = linear;
    native.mipmaps = useMipmaps;
    native.clamp = clamp;
    refreshSamplerObject(native);
    return true;
#else
    (void)texture; (void)linear; (void)mipmaps; (void)clamp;
    return false;
#endif
}

bool wii_native_texture_set_quality(int texture, bool mipmapLinear, int anisotropy)
{
#if defined(WII_PLATFORM)
    auto it = s_textures.find(texture);
    if (it == s_textures.end() || it->second.pixels == nullptr)
        return false;

    NativeTexture &native = it->second;
    const int normalizedAnisotropy = anisotropy < 1 ? 1 : (anisotropy > 4 ? 4 : anisotropy);
    if (native.mipmapLinear == mipmapLinear && native.anisotropy == normalizedAnisotropy)
        return true;

    native.mipmapLinear = mipmapLinear;
    native.anisotropy = normalizedAnisotropy;
    refreshSamplerObject(native);
    return true;
#else
    (void)texture; (void)mipmapLinear; (void)anisotropy;
    return false;
#endif
}

bool wii_native_texture_upload_level_rgba(int texture, int level, int width, int height,
                                          const unsigned char *pixels)
{
#if defined(WII_PLATFORM)
    auto it = s_textures.find(texture);
    if (it == s_textures.end() || pixels == nullptr || level < 0 || level > it->second.maxLevel)
    {
#if MC_LOG_LEVEL >= 2
        MC_LOG_INFO("wii", "[WII][GX][TEXFAIL] upload id=%d level=%d size=%dx%d resident=%d pixels=%p\n",
               texture, level, width, height, it != s_textures.end() ? 1 : 0, pixels);
#endif
        return false;
    }

    NativeTexture &native = it->second;
    if (native.pixels == nullptr)
        return false;

    const int expectedWidth = native.width >> level;
    const int expectedHeight = native.height >> level;
    if (width != expectedWidth || height != expectedHeight)
    {
#if MC_LOG_LEVEL >= 2
        MC_LOG_INFO("wii", "[WII][GX][TEXFAIL] dims id=%d level=%d got=%dx%d expected=%dx%d\n",
               texture, level, width, height, expectedWidth, expectedHeight);
#endif
        return false;
    }

    const uint32_t levelOffset = mipLevelOffset(native, level);
    const uint32_t levelBytes = GX_GetTexBufferSize((u16)width, (u16)height, native.format,
                                                    GX_FALSE, 0);
    if (levelOffset + levelBytes > native.bytes)
    {
        MC_LOG_ERROR("wii", "[WII][GX][TEXFAIL] level id=%d level=%d needs %u+%u bytes of %u\n",
               texture, level, (unsigned int)levelOffset, (unsigned int)levelBytes,
               (unsigned int)native.bytes);
        return false;
    }
    unsigned char *dst = static_cast<unsigned char *>(native.pixels) + levelOffset;

    if (native.format == GX_TF_RGBA8)
        uploadRgba8(pixels, width, height, dst);
    else
        uploadRgb5A3(pixels, width, height, dst);
#if MC_LOG_LEVEL >= 2
    if (level == 0)
    {
        static unsigned int loggedLevel0 = 0;
        if (loggedLevel0 < 32)
        {
            unsigned int minAlpha = 255, maxAlpha = 0, passAlpha = 0;
            const size_t texels = (size_t)width * (size_t)height;
            for (size_t i = 0; i < texels; ++i)
            {
                const unsigned int a = pixels[i * 4u + 3u];
                if (a < minAlpha) minAlpha = a;
                if (a > maxAlpha) maxAlpha = a;
                if (a > 25u) ++passAlpha;
            }
            MC_LOG_INFO("wii", "[WII][GX][TEXDATA] n=%u id=%d alpha=%u..%u pass>25=%u/%u\n",
                   loggedLevel0, texture, minAlpha, maxAlpha, passAlpha,
                   (unsigned int)texels);
            ++loggedLevel0;
        }
    }
#endif
    const uint32_t levelSize = GX_GetTexBufferSize((u16)width, (u16)height, native.format,
                                                   GX_FALSE, 0);
    DCFlushRange(dst, levelSize);
    markTexelCacheDirty();
    return true;
#else
    (void)texture; (void)level; (void)width; (void)height; (void)pixels;
    return false;
#endif
}

bool wii_native_texture_upload_sub_rgba(int texture, int level, int x, int y,
                                        int width, int height, const unsigned char *pixels)
{
#if defined(WII_PLATFORM)
    auto it = s_textures.find(texture);
    if (it == s_textures.end() || pixels == nullptr || level < 0 ||
        level > it->second.maxLevel || width <= 0 || height <= 0)
        return false;

    NativeTexture &native = it->second;
    if (native.pixels == nullptr)
        return false;

    const int levelWidth = native.width >> level;
    const int levelHeight = native.height >> level;
    if (x < 0 || y < 0)
        return false;
    const std::int64_t endX = static_cast<std::int64_t>(x) + static_cast<std::int64_t>(width);
    const std::int64_t endY = static_cast<std::int64_t>(y) + static_cast<std::int64_t>(height);
    if (endX > static_cast<std::int64_t>(levelWidth) ||
        endY > static_cast<std::int64_t>(levelHeight))
        return false;

    unsigned char *dst = static_cast<unsigned char *>(native.pixels) + mipLevelOffset(native, level);

    if (native.format == GX_TF_RGBA8)
        uploadRgba8SubRect(pixels, width, height, levelWidth, levelHeight, x, y, dst);
    else
        uploadRgb5A3SubRect(pixels, width, height, levelWidth, levelHeight, x, y, dst);
    const uint32_t levelSize = GX_GetTexBufferSize((u16)levelWidth, (u16)levelHeight,
                                                   native.format, GX_FALSE, 0);
    DCFlushRange(dst, levelSize);
    markTexelCacheDirty();
    return true;
#else
    (void)texture; (void)level; (void)x; (void)y; (void)width; (void)height; (void)pixels;
    return false;
#endif
}

bool wii_native_texture_bind(int texture)
{
#if defined(WII_PLATFORM)
    if (texture < 0)
        return false;
    s_currentTexture = texture;
    wii_gx_native_set_bound_texture_id((unsigned int)texture);
    return true;
#else
    (void)texture;
    return false;
#endif
}

void wii_native_texture_remove(int texture)
{
#if defined(WII_PLATFORM)
    auto it = s_textures.find(texture);
    if (it != s_textures.end()) {
        releaseTexture(it->second);
        s_textures.erase(it);
    }
    if (s_currentTexture == texture)
        s_currentTexture = -1;
    wii_gx_native_forget_texture_id(texture);
#else
    (void)texture;
#endif
}

void wii_native_texture_clear_all()
{
#if defined(WII_PLATFORM)
    for (auto &entry : s_textures)
        releaseTexture(entry.second);
    s_textures.clear();
    s_currentTexture = -1;
    wii_gx_native_clear_bound_texture_ids();
#endif
}

#if defined(WII_PLATFORM)
bool wii_native_texture_get_current(int *texture, GXTexObj *texobj)
{
    if (s_currentTexture < 0 || texobj == nullptr)
        return false;
    auto it = s_textures.find(s_currentTexture);
    if (it == s_textures.end())
        return false;
    if (texture != nullptr)
        *texture = s_currentTexture;
    *texobj = it->second.object;
    return true;
}


extern "C" int wii_native_texture_get_texobj_c(int texture, GXTexObj *texobj)
{
    if (texobj == nullptr)
        return 0;
    auto it = s_textures.find(texture);
    if (it == s_textures.end())
        return 0;
    flushTexelCacheIfDirty();
    *texobj = it->second.object;
    return 1;
}
#endif
