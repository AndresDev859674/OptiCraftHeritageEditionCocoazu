#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include "platform/PlatformTuning.h"
#include "java/Type.h"

// Terrain noise storage follows the arithmetic precision selected by the platform
// profile. Normal PC/Wii remain double; PS2 and PC Legacy use float so the large
// temporary fields match their low-CPU generation paths.
#if PLATFORM_FLOAT_TERRAIN_NOISE
using terrain_noise_real_t = float;
#else
using terrain_noise_real_t = double;
#endif

#if PLATFORM_FLOAT_BIOME_NOISE
using biome_noise_real_t = float;
#else
using biome_noise_real_t = double;
#endif

#if PLATFORM_FLOAT_TERRAIN_NOISE
using terrain_coord_real_t = float;
#else
using terrain_coord_real_t = double;
#endif

using TerrainNoiseBuffer = std::vector<terrain_noise_real_t>;
using BiomeNoiseBuffer = std::vector<biome_noise_real_t>;

inline std::size_t checkedNoiseVolumeSize(int_t sizeX, int_t sizeY, int_t sizeZ)
{
	if (sizeX < 0 || sizeY < 0 || sizeZ < 0)
		throw std::length_error("Negative noise volume dimension");

	const std::uint64_t x = static_cast<std::uint64_t>(sizeX);
	const std::uint64_t y = static_cast<std::uint64_t>(sizeY);
	const std::uint64_t z = static_cast<std::uint64_t>(sizeZ);
	const std::uint64_t volume = x * y * z;
	if (volume > static_cast<std::uint64_t>(std::numeric_limits<int_t>::max()) ||
		volume > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
		throw std::length_error("Noise volume exceeds Java array range");
	return static_cast<std::size_t>(volume);
}
