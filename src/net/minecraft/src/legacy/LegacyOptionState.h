#pragma once

#include <string>
#include "java/Type.h"

bool legacyCloudsChecked(int_t optifineCloudMode);
int_t legacyCloudsToggledValue(int_t optifineCloudMode);
bool legacyFogChecked(bool fogOff);
bool legacyFogToggledOff(bool fogOff);
bool legacySmoothLightingChecked(float_t aoLevel);
float_t legacySmoothLightingToggleValue(float_t aoLevel);
std::string legacyRenderDistanceLabel(int_t fineDistanceBlocks);

std::string legacyFovLabel(float_t normalizedFov);
std::string legacySensitivityLabel(float_t normalizedSensitivity);
