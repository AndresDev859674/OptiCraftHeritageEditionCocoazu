#pragma once

#ifdef PS2_PLATFORM

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Ps2SaveFileSystem
{
    bool isMemoryCardPath(const std::string& path);
    bool isDisabledPath(const std::string& path);
    bool mkdirs(const std::string& path);
    bool writeFile(const std::string& path, const void* data, std::size_t length);
    bool appendFile(const std::string& path, const void* data, std::size_t length);
    bool readFile(const std::string& path, std::vector<unsigned char>& out);
    std::int64_t getFileSize(const std::string& path);
    bool readFileRange(const std::string& path, std::size_t offset, void* out, std::size_t length);
    bool exists(const std::string& path);
    bool removeFile(const std::string& path);
    bool listDirs(const std::string& path, std::vector<std::string>& out);
bool isDirectory(const std::string& path);
}

#endif // PS2_PLATFORM
