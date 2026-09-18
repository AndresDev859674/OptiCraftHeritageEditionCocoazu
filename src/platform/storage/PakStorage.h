#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// PlatformStorage's read-side operations answered from the mounted assets.pak
// for "pak://" paths (see AssetPak). Each StorageBackend_* checks
// AssetPak::isPakPath() first and delegates here, so save-format code that
// only knows PlatformStorage -- SaveFormatOld, SaveHandler, RegionFile -- can
// open a world that ships inside the pak, such as the legacy tutorial, with
// no changes of its own. Directories are implied by the entry names; writes
// are not offered: a pak-backed world is opened read-only.
namespace PakStorage
{
bool exists(const std::string &path);
bool isDirectory(const std::string &path);
bool readFile(const std::string &path, std::vector<unsigned char> &out);
std::int64_t fileSize(const std::string &path);
bool readFileRange(const std::string &path, std::size_t offset, void *out, std::size_t length);
bool listEntries(const std::string &path, std::vector<std::string> &out);
bool listDirs(const std::string &path, std::vector<std::string> &out);
}
