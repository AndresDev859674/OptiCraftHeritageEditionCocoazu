#include "LegacyPanoramaUpload.h"

#include <algorithm>
#include <utility>

#include "java/BufferedImage.h"
#include "java/Type.h"
#include "platform/Log.h"

constexpr int_t LEGACY_PANORAMA_PS2_MAX_WIDTH = 512;
constexpr int_t LEGACY_PANORAMA_WII_MAX_WIDTH = 1024;

std::unique_ptr<BufferedImage> legacyPreparePanoramaForUpload(
	const std::string &name, std::unique_ptr<BufferedImage> image)
{
	if (!image || name != "/legacy/panorama.png")
		return image;

	int_t maxWidth = 0;
#if defined(PS2_PLATFORM)
	maxWidth = LEGACY_PANORAMA_PS2_MAX_WIDTH;
#elif defined(WII_PLATFORM)
	maxWidth = LEGACY_PANORAMA_WII_MAX_WIDTH;
#else
	return image;
#endif

	const int_t sourceWidth = image->getWidth();
	const int_t sourceHeight = image->getHeight();
	if (sourceWidth <= 0 || sourceHeight <= 0 || sourceWidth <= maxWidth)
		return image;

	const int_t targetWidth = maxWidth;
	const long long scaledHeight =
		static_cast<long long>(sourceHeight) * static_cast<long long>(targetWidth);
	const int_t targetHeight = std::max<int_t>(1, static_cast<int_t>(
		(scaledHeight + sourceWidth / 2) / sourceWidth));

	const unsigned char *src = image->getRawPixels();
	std::unique_ptr<unsigned char[]> dst(
		new unsigned char[BufferedImage::checkedRgbaByteCount(targetWidth, targetHeight)]);

	// Bilinear resampling in 16.16 fixed point keeps this one-time console path
	// cheap and avoids introducing another image-resize dependency. The source
	// BufferedImage is released as soon as the resized image is returned, before
	// setupTexture() allocates its upload staging buffer.
	for (int_t y = 0; y < targetHeight; ++y)
	{
		const unsigned int yFixed = targetHeight > 1
			? static_cast<unsigned int>((static_cast<unsigned long long>(y) *
				static_cast<unsigned long long>(sourceHeight - 1) << 16) /
				static_cast<unsigned int>(targetHeight - 1))
			: 0u;
		const int_t y0 = static_cast<int_t>(yFixed >> 16);
		const int_t y1 = std::min<int_t>(y0 + 1, sourceHeight - 1);
		const unsigned int fy = yFixed & 0xffffu;

		for (int_t x = 0; x < targetWidth; ++x)
		{
			const unsigned int xFixed = targetWidth > 1
				? static_cast<unsigned int>((static_cast<unsigned long long>(x) *
					static_cast<unsigned long long>(sourceWidth - 1) << 16) /
					static_cast<unsigned int>(targetWidth - 1))
				: 0u;
			const int_t x0 = static_cast<int_t>(xFixed >> 16);
			const int_t x1 = std::min<int_t>(x0 + 1, sourceWidth - 1);
			const unsigned int fx = xFixed & 0xffffu;

			const std::size_t p00 = (static_cast<std::size_t>(y0) * sourceWidth + x0) * 4u;
			const std::size_t p10 = (static_cast<std::size_t>(y0) * sourceWidth + x1) * 4u;
			const std::size_t p01 = (static_cast<std::size_t>(y1) * sourceWidth + x0) * 4u;
			const std::size_t p11 = (static_cast<std::size_t>(y1) * sourceWidth + x1) * 4u;
			const std::size_t out = (static_cast<std::size_t>(y) * targetWidth + x) * 4u;

			for (int_t channel = 0; channel < 4; ++channel)
			{
				const unsigned int top =
					(static_cast<unsigned int>(src[p00 + channel]) * (0x10000u - fx) +
					 static_cast<unsigned int>(src[p10 + channel]) * fx + 0x8000u) >> 16;
				const unsigned int bottom =
					(static_cast<unsigned int>(src[p01 + channel]) * (0x10000u - fx) +
					 static_cast<unsigned int>(src[p11 + channel]) * fx + 0x8000u) >> 16;
				dst[out + channel] = static_cast<unsigned char>(
					(top * (0x10000u - fy) + bottom * fy + 0x8000u) >> 16);
			}
		}
	}

	MC_LOG_DEBUG("render", "legacy panorama upload resize %dx%d -> %dx%d\n",
		(int)sourceWidth, (int)sourceHeight, (int)targetWidth, (int)targetHeight);
	return std::unique_ptr<BufferedImage>(
		new BufferedImage(targetWidth, targetHeight, std::move(dst)));
}
