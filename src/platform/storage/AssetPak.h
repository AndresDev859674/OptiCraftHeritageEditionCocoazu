#pragma once

#include <cstdint>
#include <istream>
#include <memory>
#include <string>
#include <vector>

// The game's mounted assets.pak (see PakArchive), addressed by the same keys
// the loose data/ tree uses: "assets/gui/items.png", "resources/sound/...".
//
// Integration is by path: PlatformResources::resolveExisting() answers with a
// "pak://<key>" string when the pak holds the key, and the readers that used
// to fopen/ifstream the resolved path ask here instead when isPakPath() says
// so. A missing pak, or a key the pak does not hold, falls through to the
// loose file exactly as before, so an install with only the data/ tree keeps
// working and a pak can be partial.
namespace AssetPak
{

// Opens <baseDir>/assets.pak (or <device:>assets.pak for a bare device
// prefix). While nothing is mounted every call tries again, so a locator can
// offer several roots in turn; once a pak is mounted only its own root is
// answered true. Returns whether THAT pak is the mounted one.
bool mountFrom(const std::string &baseDir);

// Mounts a pak using an exact filesystem path. CD/DVD discovery uses this to
// preserve the ISO9660 case and version suffix selected by its resolver.
bool mountFile(const std::string &path);
bool mounted();
const std::string &archivePath();

bool isPakPath(const std::string &path);
std::string makePath(const std::string &key);
std::string keyOf(const std::string &pakPath);

bool exists(const std::string &key);
long size(const std::string &key);
bool isDirectory(const std::string &key);
bool listChildren(const std::string &directoryKey, std::vector<std::string> &out);

// Whole entry in a free()-able buffer, 64-byte aligned on the consoles.
unsigned char *load(const std::string &key, unsigned int *outSize);
bool read(const std::string &key, std::uint32_t offset, void *dst, std::uint32_t length);

// Byte range of the entry inside archivePath(), for readers that keep their
// own handle on the pak (audio streams on their own thread).
bool locate(const std::string &key, std::uint32_t *dataOffset, std::uint32_t *size);

// Sequential istream over the entry: whole entry in memory below
// kStreamInMemoryBytes, a windowed reader above it.
std::unique_ptr<std::istream> openStream(const std::string &key);
const std::uint32_t kStreamInMemoryBytes = 256u * 1024u;

}
