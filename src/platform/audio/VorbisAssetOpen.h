#pragma once

// stb_vorbis openers that accept a "pak://" path (AssetPak) as well as a
// loose file. A pak entry is opened as a file section on a handle of the
// caller's own -- stb_vorbis owns and closes it -- so a decoder on the audio
// thread never shares a file position with the main thread's loader.

#include "pc/external/stb_vorbis.h"
#include "platform/storage/AssetPak.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

inline stb_vorbis *platformOpenVorbis(const std::string &path, int *error)
{
    if (!AssetPak::isPakPath(path))
        return stb_vorbis_open_filename(path.c_str(), error, nullptr);

    std::uint32_t dataOffset = 0;
    std::uint32_t size = 0;
    if (!AssetPak::locate(AssetPak::keyOf(path), &dataOffset, &size))
        return nullptr;
    std::FILE *file = std::fopen(AssetPak::archivePath().c_str(), "rb");
    if (file == nullptr)
        return nullptr;
    if (std::fseek(file, static_cast<long>(dataOffset), SEEK_SET) != 0)
    {
        std::fclose(file);
        return nullptr;
    }
    stb_vorbis *vorbis = stb_vorbis_open_file_section(file, 1, error, nullptr,
                                                      static_cast<unsigned int>(size));
    if (vorbis == nullptr)
        std::fclose(file);
    return vorbis;
}

// stb_vorbis_decode_filename() for either kind of path: whole stream to
// interleaved 16-bit PCM in a malloc'd buffer the caller frees. Returns the
// sample count per channel, or -1.
inline int platformDecodeVorbis(const std::string &path, int *channels, int *sampleRate, short **output)
{
    int error = 0;
    stb_vorbis *vorbis = platformOpenVorbis(path, &error);
    if (vorbis == nullptr)
        return -1;

    const stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    const unsigned int frames = stb_vorbis_stream_length_in_samples(vorbis);
    if (info.channels <= 0 || frames == 0)
    {
        stb_vorbis_close(vorbis);
        return -1;
    }
    short *data = static_cast<short *>(std::malloc(static_cast<std::size_t>(frames) *
                                                   static_cast<std::size_t>(info.channels) * sizeof(short)));
    if (data == nullptr)
    {
        stb_vorbis_close(vorbis);
        return -1;
    }
    int decoded = 0;
    while (decoded < static_cast<int>(frames))
    {
        const int got = stb_vorbis_get_samples_short_interleaved(
            vorbis, info.channels, data + static_cast<std::size_t>(decoded) * info.channels,
            (static_cast<int>(frames) - decoded) * info.channels);
        if (got <= 0)
            break;
        decoded += got;
    }
    stb_vorbis_close(vorbis);
    if (decoded <= 0)
    {
        std::free(data);
        return -1;
    }
    if (channels != nullptr)
        *channels = info.channels;
    if (sampleRate != nullptr)
        *sampleRate = static_cast<int>(info.sample_rate);
    *output = data;
    return decoded;
}
