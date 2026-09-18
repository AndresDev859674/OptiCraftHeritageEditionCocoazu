#pragma once

#include <algorithm>
#include <cmath>

#include "net/minecraft/src/EntityLiving.h"

struct AudioListenerState
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline void updateAudioListener(AudioListenerState &listener, const EntityLiving *entity, float partialTick)
{
    if (entity == nullptr)
        return;

    listener.x = static_cast<float>(entity->prevPosX + (entity->posX - entity->prevPosX) * partialTick);
    listener.y = static_cast<float>(entity->prevPosY + (entity->posY - entity->prevPosY) * partialTick);
    listener.z = static_cast<float>(entity->prevPosZ + (entity->posZ - entity->prevPosZ) * partialTick);
}

inline float audioStreamingAttenuation(const AudioListenerState &listener,
                                      float sourceX,
                                      float sourceY,
                                      float sourceZ)
{
    constexpr float maxDistance = 64.0f;
    const float dx = sourceX - listener.x;
    const float dy = sourceY - listener.y;
    const float dz = sourceZ - listener.z;
    const float distanceSq = dx * dx + dy * dy + dz * dz;
    const float maxDistanceSq = maxDistance * maxDistance;

    if (distanceSq >= maxDistanceSq)
        return 0.0f;

    const float distance = std::sqrt(distanceSq);
    return std::max(0.0f, 1.0f - distance / maxDistance);
}

inline float audioSpatialAttenuation(const AudioListenerState &listener,
                                     float sourceX,
                                     float sourceY,
                                     float sourceZ,
                                     float sourceVolume)
{
    float maxDistance = 16.0f;
    if (sourceVolume > 1.0f)
        maxDistance *= sourceVolume;

    const float dx = sourceX - listener.x;
    const float dy = sourceY - listener.y;
    const float dz = sourceZ - listener.z;
    const float distanceSq = dx * dx + dy * dy + dz * dz;
    const float maxDistanceSq = maxDistance * maxDistance;

    if (distanceSq >= maxDistanceSq)
        return 0.0f;

    const float distance = std::sqrt(distanceSq);
    return std::max(0.0f, 1.0f - distance / maxDistance);
}
