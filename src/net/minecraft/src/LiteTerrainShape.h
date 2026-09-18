#pragma once

namespace LiteTerrain
{
struct BiomeShape
{
	float baseHeight;
	float heightVariation;
};

struct BiomeBlendSample
{
	BiomeShape shape;
	float inverseWeightDenominator;
};

inline float clamp(float value, float minimum, float maximum)
{
	if (value < minimum)
		return minimum;
	if (value > maximum)
		return maximum;
	return value;
}

inline BiomeBlendSample makeBiomeBlendSample(const BiomeShape &shape)
{
	const float denominator = clamp(shape.baseHeight + 2.0f, 0.25f, 4.0f);
	return BiomeBlendSample{shape, 1.0f / denominator};
}

template <typename Getter>
BiomeShape blendBiomeShape(int centerX, int centerZ, Getter getBiomeSample)
{
	static constexpr float kernel[25] = {
		1.0f, 2.0f, 3.0f, 2.0f, 1.0f,
		2.0f, 4.0f, 6.0f, 4.0f, 2.0f,
		3.0f, 6.0f, 9.0f, 6.0f, 3.0f,
		2.0f, 4.0f, 6.0f, 4.0f, 2.0f,
		1.0f, 2.0f, 3.0f, 2.0f, 1.0f
	};

	const BiomeShape center = getBiomeSample(centerX, centerZ).shape;
	float baseHeight = 0.0f;
	float heightVariation = 0.0f;
	float totalWeight = 0.0f;

	for (int offsetZ = -2; offsetZ <= 2; ++offsetZ)
	{
		for (int offsetX = -2; offsetX <= 2; ++offsetX)
		{
			const BiomeBlendSample &neighbor = getBiomeSample(centerX + offsetX, centerZ + offsetZ);
			const int kernelIndex = (offsetZ + 2) * 5 + offsetX + 2;
			float weight = kernel[kernelIndex] * neighbor.inverseWeightDenominator;
			if (neighbor.shape.baseHeight > center.baseHeight)
				weight *= 0.5f;

			baseHeight += neighbor.shape.baseHeight * weight;
			heightVariation += neighbor.shape.heightVariation * weight;
			totalWeight += weight;
		}
	}

	if (totalWeight <= 0.0f)
		return center;
	return BiomeShape{baseHeight / totalWeight, heightVariation / totalWeight};
}

// Interpolates four lattice-point blends for a column inside the cell they
// span. fractionX/fractionZ are the column's offsets across the cell in 0..1.
inline BiomeShape bilinearBiomeShape(const BiomeShape &shape00, const BiomeShape &shape10,
	const BiomeShape &shape01, const BiomeShape &shape11, float fractionX, float fractionZ)
{
	const float weight00 = (1.0f - fractionX) * (1.0f - fractionZ);
	const float weight10 = fractionX * (1.0f - fractionZ);
	const float weight01 = (1.0f - fractionX) * fractionZ;
	const float weight11 = fractionX * fractionZ;
	return BiomeShape{
		shape00.baseHeight * weight00 + shape10.baseHeight * weight10 +
			shape01.baseHeight * weight01 + shape11.baseHeight * weight11,
		shape00.heightVariation * weight00 + shape10.heightVariation * weight10 +
			shape01.heightVariation * weight01 + shape11.heightVariation * weight11};
}

inline float surfaceHeight(float baseHeight, float amplitude, float continentalNoise,
	float detailNoise, const BiomeShape &biome)
{
	const float biomeOffset = clamp(biome.baseHeight * 16.0f, -18.0f, 24.0f);
	float relief = clamp(biome.heightVariation * 0.75f + 0.15f, 0.15f, 1.15f);
	if (biome.baseHeight < 0.0f)
		relief *= 0.55f;

	const float detailAmplitude = amplitude * (0.12f + relief * 0.12f);
	return baseHeight + biomeOffset + continentalNoise * amplitude * relief +
		detailNoise * detailAmplitude;
}
}
