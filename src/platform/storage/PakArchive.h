#pragma once

#include "platform/storage/PakFile.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// Reader for the assets.pak container written by scripts/make_pak.py.
//
// One uncompressed file, opened once: a sorted table of (hash, name, offset,
// size) up front, file bytes behind it. Lookups bisect the table in memory;
// reads are a seek and a read on the one handle, so loading a texture no
// longer walks the FAT directory chain through the IOP (PS2) or libfat (Wii)
// for every file. Keys are the path relative to the packed data/ directory
// ("assets/gui/items.png"), matched case-insensitively.
//
// Layout, all integers big-endian u32:
//   header 32 bytes  'MCPK', version 1, entryCount, tableOffset, namesOffset,
//                    namesBytes, dataAlign, reserved
//   table            entryCount x 16 bytes: hash, nameOffset, dataOffset, size,
//                    sorted by (hash, name)
//   names            NUL-terminated keys
//   data             each entry at a dataAlign boundary
class PakArchive
{
public:
    struct Entry
    {
        std::uint32_t hash;
        std::uint32_t nameOffset;
        std::uint32_t dataOffset;
        std::uint32_t size;
    };

    PakArchive();
    ~PakArchive();
    PakArchive(const PakArchive &) = delete;
    PakArchive &operator=(const PakArchive &) = delete;

    // Reads the header, table and names; keeps the file open for read().
    bool open(const std::string &path);
    void close();
    bool isOpen() const;
    const std::string &path() const;

    // Key as the caller has it: slashes either way, optional leading slash.
    const Entry *find(const std::string &key) const;
    const char *name(const Entry &entry) const;
    std::size_t entryCount() const;
    const Entry &entryAt(std::size_t index) const;

    // Reads [offset, offset + length) of the entry's bytes. Serialised on the
    // shared handle; streaming readers that want their own file position open
    // the pak path themselves and use the entry's dataOffset (see AssetPak).
    bool read(const Entry &entry, std::uint32_t offset, void *dst, std::uint32_t length);

    // Immediate children of a directory key ("resources/sound"): file names
    // and subdirectory names, unqualified, each once. False when no entry
    // lies under the prefix.
    bool listChildren(const std::string &directoryKey, std::vector<std::string> &out) const;

    // "a\\b//c" -> "a/b/c"; no leading slash. Lowercase for hashing/matching.
    static std::string normalizeKey(const std::string &key);
    static std::uint32_t hashKey(const std::string &normalizedLowercaseKey);

private:
    static bool equalsIgnoreCase(const char *a, const std::string &b);

    std::string path_;
    PakFile file_;
    std::vector<Entry> entries_;
    std::vector<char> names_;
    std::mutex readMutex_;
};
