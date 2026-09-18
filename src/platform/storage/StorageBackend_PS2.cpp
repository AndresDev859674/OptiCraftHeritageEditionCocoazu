#include "platform/Storage.h"
#include "platform/storage/AssetPak.h"
#include "platform/storage/PakStorage.h"
#include "ps2/storage/save/Ps2SaveFileSystem.h"

// "pak://" paths are the mounted assets.pak (read-only; see PakStorage), so a
// world shipped inside it -- the legacy tutorial -- opens through the same
// save-format code as one on the memory card or USB device.
namespace PlatformStorage
{
bool exists(const std::string& path)
{
    if (AssetPak::isPakPath(path))
        return PakStorage::exists(path);
    return Ps2SaveFileSystem::exists(path);
}
bool mkdirs(const std::string& path)
{
    // Nothing to create inside the pak; the directory is there if entries are.
    if (AssetPak::isPakPath(path))
        return PakStorage::isDirectory(path);
    return Ps2SaveFileSystem::mkdirs(path);
}
bool removeFile(const std::string& path) { return Ps2SaveFileSystem::removeFile(path); }
bool renameFile(const std::string& from, const std::string& to)
{
    (void)from;
    (void)to;
    return false;
}
bool supportsAtomicRename() { return false; }
bool supportsSessionLocks() { return false; }
bool readFile(const std::string& path, std::vector<unsigned char>& out)
{
    if (AssetPak::isPakPath(path))
        return PakStorage::readFile(path, out);
    return Ps2SaveFileSystem::readFile(path, out);
}
std::int64_t getFileSize(const std::string& path)
{
    if (AssetPak::isPakPath(path))
        return PakStorage::fileSize(path);
    return Ps2SaveFileSystem::getFileSize(path);
}
bool readFileRange(const std::string& path, std::size_t offset, void* out, std::size_t length)
{
    if (AssetPak::isPakPath(path))
        return PakStorage::readFileRange(path, offset, out, length);
    return Ps2SaveFileSystem::readFileRange(path, offset, out, length);
}
bool writeFile(const std::string& path, const void* data, std::size_t length) { return Ps2SaveFileSystem::writeFile(path, data, length); }
bool appendFile(const std::string& path, const void* data, std::size_t length) { return Ps2SaveFileSystem::appendFile(path, data, length); }
bool pathIsDirectory(const std::string& path)
{
    if (AssetPak::isPakPath(path))
        return PakStorage::isDirectory(path);
    return Ps2SaveFileSystem::isDirectory(path);
}
bool listPathEntries(const std::string& path, std::vector<std::string>& out)
{
    if (AssetPak::isPakPath(path))
        return PakStorage::listEntries(path, out);
    return Ps2SaveFileSystem::listDirs(path, out);
}
bool listDirs(const std::string& path, std::vector<std::string>& out)
{
    if (AssetPak::isPakPath(path))
        return PakStorage::listDirs(path, out);
    return Ps2SaveFileSystem::listDirs(path, out);
}
}
