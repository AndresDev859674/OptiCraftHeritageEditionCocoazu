#include "platform/storage/PakStorage.h"

#include "platform/storage/AssetPak.h"

#include <cstdlib>
#include <cstring>

namespace PakStorage
{

bool exists(const std::string &path)
{
    const std::string key = AssetPak::keyOf(path);
    return AssetPak::exists(key) || AssetPak::isDirectory(key);
}

bool isDirectory(const std::string &path)
{
    return AssetPak::isDirectory(AssetPak::keyOf(path));
}

bool readFile(const std::string &path, std::vector<unsigned char> &out)
{
    unsigned int size = 0;
    unsigned char *data = AssetPak::load(AssetPak::keyOf(path), &size);
    if (data == nullptr)
        return false;
    out.assign(data, data + size);
    std::free(data);
    return true;
}

std::int64_t fileSize(const std::string &path)
{
    return static_cast<std::int64_t>(AssetPak::size(AssetPak::keyOf(path)));
}

bool readFileRange(const std::string &path, std::size_t offset, void *out, std::size_t length)
{
    if (offset > 0xFFFFFFFFu || length > 0xFFFFFFFFu)
        return false;
    return AssetPak::read(AssetPak::keyOf(path), static_cast<std::uint32_t>(offset), out,
                          static_cast<std::uint32_t>(length));
}

bool listEntries(const std::string &path, std::vector<std::string> &out)
{
    out.clear();
    return AssetPak::listChildren(AssetPak::keyOf(path), out);
}

bool listDirs(const std::string &path, std::vector<std::string> &out)
{
    std::vector<std::string> entries;
    out.clear();
    if (!listEntries(path, entries))
        return false;
    const std::string key = AssetPak::keyOf(path);
    for (const std::string &entry : entries)
    {
        if (AssetPak::isDirectory(key + "/" + entry))
            out.push_back(entry);
    }
    return true;
}

}
