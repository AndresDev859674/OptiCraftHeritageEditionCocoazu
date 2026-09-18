#include "platform/storage/PakArchive.h"

#include "platform/Log.h"

#include <algorithm>
#include <cstring>

namespace
{
const char kMagic[4] = { 'M', 'C', 'P', 'K' };
const std::uint32_t kVersion = 1;
const std::size_t kHeaderBytes = 32;
const std::size_t kEntryBytes = 16;
// Table + names ceiling: a pak of 100k entries would be far beyond any asset
// tree this game ships, so anything larger is a corrupt header, not data.
const std::uint32_t kMaxEntries = 100000;
const std::uint32_t kMaxNamesBytes = 16u * 1024u * 1024u;

std::uint32_t readU32(const unsigned char *p)
{
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) |
           static_cast<std::uint32_t>(p[3]);
}

char lowerAscii(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}
}

PakArchive::PakArchive()
{
}

PakArchive::~PakArchive()
{
    close();
}

bool PakArchive::open(const std::string &path)
{
    close();
    if (!file_.open(path))
        return false;

    unsigned char header[kHeaderBytes];
    if (!file_.readAt(0, header, kHeaderBytes) ||
        std::memcmp(header, kMagic, sizeof(kMagic)) != 0 ||
        readU32(header + 4) != kVersion)
    {
        MC_LOG_WARN("assets", "[pak] %s: not a version %u pak\n", path.c_str(), (unsigned)kVersion);
        file_.close();
        return false;
    }

    const std::uint32_t entryCount = readU32(header + 8);
    const std::uint32_t tableOffset = readU32(header + 12);
    const std::uint32_t namesOffset = readU32(header + 16);
    const std::uint32_t namesBytes = readU32(header + 20);
    if (entryCount > kMaxEntries || namesBytes > kMaxNamesBytes ||
        tableOffset < kHeaderBytes || namesOffset < tableOffset + entryCount * kEntryBytes)
    {
        MC_LOG_WARN("assets", "[pak] %s: header out of range\n", path.c_str());
        file_.close();
        return false;
    }

    std::vector<unsigned char> table(static_cast<std::size_t>(entryCount) * kEntryBytes);
    if (!table.empty() && !file_.readAt(tableOffset, table.data(), static_cast<std::uint32_t>(table.size())))
    {
        file_.close();
        return false;
    }
    std::vector<char> names(static_cast<std::size_t>(namesBytes) + 1u, '\0');
    if (namesBytes != 0 && !file_.readAt(namesOffset, names.data(), namesBytes))
    {
        file_.close();
        return false;
    }

    entries_.resize(entryCount);
    for (std::uint32_t i = 0; i < entryCount; ++i)
    {
        const unsigned char *record = table.data() + static_cast<std::size_t>(i) * kEntryBytes;
        Entry &entry = entries_[i];
        entry.hash = readU32(record);
        entry.nameOffset = readU32(record + 4);
        entry.dataOffset = readU32(record + 8);
        entry.size = readU32(record + 12);
        if (entry.nameOffset >= namesBytes)
        {
            MC_LOG_WARN("assets", "[pak] %s: entry %u name out of range\n", path.c_str(), (unsigned)i);
            entries_.clear();
            file_.close();
            return false;
        }
    }
    names_.swap(names);
    path_ = path;
    MC_LOG_INFO("assets", "[pak] %s: %u entries\n", path.c_str(), (unsigned)entryCount);
    return true;
}

void PakArchive::close()
{
    file_.close();
    path_.clear();
    entries_.clear();
    names_.clear();
}

bool PakArchive::isOpen() const
{
    return file_.isOpen();
}

const std::string &PakArchive::path() const
{
    return path_;
}

std::string PakArchive::normalizeKey(const std::string &key)
{
    std::string out;
    out.reserve(key.size());
    for (char c : key)
    {
        if (c == '\\')
            c = '/';
        if (c == '/' && (out.empty() || out.back() == '/'))
            continue;
        out.push_back(c);
    }
    // "./x" from callers that build paths relative to a root.
    while (out.compare(0, 2, "./") == 0)
        out.erase(0, 2);
    return out;
}

std::uint32_t PakArchive::hashKey(const std::string &normalizedLowercaseKey)
{
    std::uint32_t value = 0x811C9DC5u;
    for (unsigned char c : normalizedLowercaseKey)
    {
        value ^= c;
        value *= 0x01000193u;
    }
    return value;
}

bool PakArchive::equalsIgnoreCase(const char *a, const std::string &b)
{
    std::size_t i = 0;
    for (; i < b.size(); ++i)
    {
        if (a[i] == '\0' || lowerAscii(a[i]) != lowerAscii(b[i]))
            return false;
    }
    return a[i] == '\0';
}

const PakArchive::Entry *PakArchive::find(const std::string &key) const
{
    if (entries_.empty())
        return nullptr;
    std::string normalized = normalizeKey(key);
    if (normalized.empty())
        return nullptr;
    std::string lowered = normalized;
    for (char &c : lowered)
        c = lowerAscii(c);
    const std::uint32_t hash = hashKey(lowered);

    auto it = std::lower_bound(entries_.begin(), entries_.end(), hash,
        [](const Entry &entry, std::uint32_t value) { return entry.hash < value; });
    for (; it != entries_.end() && it->hash == hash; ++it)
    {
        if (equalsIgnoreCase(name(*it), lowered))
            return &*it;
    }
    return nullptr;
}

const char *PakArchive::name(const Entry &entry) const
{
    return names_.data() + entry.nameOffset;
}

std::size_t PakArchive::entryCount() const
{
    return entries_.size();
}

const PakArchive::Entry &PakArchive::entryAt(std::size_t index) const
{
    return entries_[index];
}

bool PakArchive::read(const Entry &entry, std::uint32_t offset, void *dst, std::uint32_t length)
{
    if (!file_.isOpen() || dst == nullptr)
        return false;
    if (offset > entry.size || length > entry.size - offset)
        return false;
    if (length == 0)
        return true;
    std::lock_guard<std::mutex> guard(readMutex_);
    return file_.readAt(entry.dataOffset + offset, dst, length);
}

bool PakArchive::listChildren(const std::string &directoryKey, std::vector<std::string> &out) const
{
    std::string prefix = normalizeKey(directoryKey);
    if (!prefix.empty() && prefix.back() != '/')
        prefix.push_back('/');
    std::string loweredPrefix = prefix;
    for (char &c : loweredPrefix)
        c = lowerAscii(c);

    bool any = false;
    for (const Entry &entry : entries_)
    {
        const char *entryName = name(entry);
        std::size_t i = 0;
        for (; i < loweredPrefix.size(); ++i)
        {
            if (entryName[i] == '\0' || lowerAscii(entryName[i]) != loweredPrefix[i])
                break;
        }
        if (i != loweredPrefix.size())
            continue;
        any = true;
        const char *rest = entryName + loweredPrefix.size();
        const char *slash = std::strchr(rest, '/');
        std::string child = slash != nullptr ? std::string(rest, slash - rest) : std::string(rest);
        if (child.empty())
            continue;
        if (std::find(out.begin(), out.end(), child) == out.end())
            out.push_back(child);
    }
    return any;
}
