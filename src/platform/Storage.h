#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace PlatformStorage
{
bool exists(const std::string& path);
bool mkdirs(const std::string& path);
bool removeFile(const std::string& path);
bool renameFile(const std::string& from, const std::string& to);
bool supportsAtomicRename();
bool supportsSessionLocks();
bool readFile(const std::string& path, std::vector<unsigned char>& out);
std::int64_t getFileSize(const std::string& path);
bool readFileRange(const std::string& path, std::size_t offset, void* out, std::size_t length);
bool writeFile(const std::string& path, const void* data, std::size_t length);
bool appendFile(const std::string& path, const void* data, std::size_t length);
bool listDirs(const std::string& path, std::vector<std::string>& out);
bool listPathEntries(const std::string& path, std::vector<std::string>& out);
bool pathIsDirectory(const std::string& path);
std::string join(const std::string& base, const std::string& child);
}
