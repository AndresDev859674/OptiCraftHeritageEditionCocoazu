#include "platform/Storage.h"
#include "platform/storage/AssetPak.h"
#include "platform/storage/PakStorage.h"
#include "platform/storage/PathUtils.h"

#include <filesystem>
#include <fstream>

// "pak://" paths are the mounted assets.pak (read-only; see PakStorage), so a
// world shipped inside it -- the legacy tutorial -- opens through the same
// save-format code as one on disk.
namespace PlatformStorage
{
bool exists(const std::string& path)
{
    if (AssetPak::isPakPath(path))
        return PakStorage::exists(path);
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

bool mkdirs(const std::string& path)
{
    // Nothing to create inside the pak; the directory is there if entries are.
    if (AssetPak::isPakPath(path))
        return PakStorage::isDirectory(path);
    std::error_code ec;
    if (path.empty())
        return false;
    if (std::filesystem::exists(path, ec))
        return true;
    return std::filesystem::create_directories(path, ec) || std::filesystem::exists(path, ec);
}

bool removeFile(const std::string& path)
{
    std::error_code ec;
    return std::filesystem::remove(path, ec);
}

bool renameFile(const std::string& from, const std::string& to)
{
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    return !ec;
}

bool supportsAtomicRename() { return true; }
bool supportsSessionLocks() { return true; }

bool readFile(const std::string& path, std::vector<unsigned char>& out)
{
    if (AssetPak::isPakPath(path))
        return PakStorage::readFile(path, out);
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size < 0)
        return false;
    file.seekg(0, std::ios::beg);
    out.resize(static_cast<std::size_t>(size));
    if (size > 0)
        file.read(reinterpret_cast<char*>(out.data()), size);
    return file.good() || file.eof();
}

bool writeFile(const std::string& path, const void* data, std::size_t length)
{
    const std::string parentDir = parent(path);
    if (!parentDir.empty() && !mkdirs(parentDir))
        return false;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
        return false;
    if (length != 0)
        file.write(static_cast<const char*>(data), static_cast<std::streamsize>(length));
    return file.good();
}

bool appendFile(const std::string& path, const void* data, std::size_t length)
{
    const std::string parentDir = parent(path);
    if (!parentDir.empty() && !mkdirs(parentDir))
        return false;
    std::ofstream file(path, std::ios::binary | std::ios::app);
    if (!file)
        return false;
    if (length != 0)
        file.write(static_cast<const char*>(data), static_cast<std::streamsize>(length));
    return file.good();
}

std::int64_t getFileSize(const std::string& path)
{
    if (AssetPak::isPakPath(path))
        return PakStorage::fileSize(path);
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    return ec ? -1 : static_cast<std::int64_t>(size);
}

bool readFileRange(const std::string& path, std::size_t offset, void* out, std::size_t length)
{
    if (AssetPak::isPakPath(path))
        return PakStorage::readFileRange(path, offset, out, length);
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;
    file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    file.read(static_cast<char*>(out), static_cast<std::streamsize>(length));
    return file.gcount() == static_cast<std::streamsize>(length);
}

bool pathIsDirectory(const std::string& path)
{
    if (AssetPak::isPakPath(path))
        return PakStorage::isDirectory(path);
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
}

bool listPathEntries(const std::string& path, std::vector<std::string>& out)
{
    if (AssetPak::isPakPath(path))
        return PakStorage::listEntries(path, out);
    out.clear();
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(path, ec))
    {
        if (ec)
            break;
        out.push_back(entry.path().filename().string());
    }
    return !ec;
}

bool listDirs(const std::string& path, std::vector<std::string>& out)
{
    if (AssetPak::isPakPath(path))
        return PakStorage::listDirs(path, out);
    out.clear();
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(path, ec))
    {
        if (ec)
            break;
        if (entry.is_directory())
            out.push_back(entry.path().filename().string());
    }
    return !ec;
}
}
