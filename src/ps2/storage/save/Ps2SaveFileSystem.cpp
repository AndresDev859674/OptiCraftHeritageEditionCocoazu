#ifdef PS2_PLATFORM

#include "ps2/storage/save/Ps2SaveFileSystem.h"
#include "ps2/storage/save/Ps2MemoryCardFileSystem.h"
#include "ps2/storage/assets/Ps2ResourceManifest.h"
#include "platform/storage/PathUtils.h"
#include "platform/storage/PosixFileSystem.h"

#include <cctype>
#include <limits>
#include <string>

namespace Ps2SaveFileSystem
{

bool isMemoryCardPath(const std::string& path)
{
    return Ps2MemoryCardFileSystem::handles(path);
}

// Enumeration is the one disc operation that does not work. readdir() on the
// IOP cdvdman/cdvdfsv driver reports success and zero entries for a real,
// non-empty directory -- confirmed by mounting the built ISO with Windows' own
// ISO9660 driver and seeing the files that "aren't there". Opening a disc file
// by name through fopen() is entirely reliable, which is why every other disc
// read in this codebase works. So answer these two from the build-time
// manifest instead of asking the disc; see Ps2ResourceManifest.
bool isDiscPath(const std::string& path)
{
    return PlatformStorage::hasPrefix(path, "cdrom") || PlatformStorage::hasPrefix(path, "cdfs:");
}

// ISO9660 permits only A-Z, 0-9 and '_' in a name, plus a single '.' before
// the extension, so an authoring tool substitutes '_' for everything else. The
// tutorial world is the case that matters here: "r.0.0.mca" is written to the
// disc as "R_0_0.MCA", "r.-1.-1.mca" as "R__1__1.MCA" and "DIM-1" as "DIM_1".
// Nothing in the recorded name says which characters were replaced, so a disc
// read cannot recover the original -- it has to ask for the substituted form.
//
// Applied per path component, because the substitution is per name: the last
// '.' survives as the extension separator and every earlier one is replaced
// like any other illegal character ("template.natural.properties" becomes
// "TEMPLATE_NATURAL.PROPERTIES").
//
// This does not rescue a name the tool TRUNCATED. ISO level 1 cuts to 8.3 and
// level 2 to 31 characters, and those losses are not reversible; build the
// disc at level 2 or higher so only substitution is in play.
std::string iso9660Name(const std::string& component)
{
    const std::size_t lastDot = component.find_last_of('.');
    std::string result;
    result.reserve(component.size());
    for (std::size_t i = 0; i < component.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(component[i]);
        if (i == lastDot)
            result.push_back('.');
        else if (std::isalnum(c) || c == '_')
            result.push_back(static_cast<char>(std::toupper(c)));
        else
            result.push_back('_');
    }
    return result;
}

std::string iso9660Path(const std::string& path)
{
    const std::size_t colon = path.find(':');
    const std::size_t start = colon == std::string::npos ? 0 : colon + 1;

    std::string result = path.substr(0, start);
    std::size_t cursor = start;
    for (;;)
    {
        const std::size_t slash = path.find('/', cursor);
        const std::size_t end = slash == std::string::npos ? path.size() : slash;
        result += iso9660Name(path.substr(cursor, end - cursor));
        if (slash == std::string::npos)
            break;
        result.push_back('/');
        cursor = slash + 1;
    }
    return result;
}

// The readable spelling of a disc file, or empty if no variant opens. The
// caller's own spelling is tried first so a disc authored with the names
// intact (ISO level 4) costs nothing extra.
std::string resolveDiscFile(const std::string& path)
{
    if (PlatformStorage::fileReadable(path))
        return path;

    const std::string versioned = path + ";1";
    if (PlatformStorage::fileReadable(versioned))
        return versioned;

    const std::string mangled = iso9660Path(path);
    if (mangled != path)
    {
        if (PlatformStorage::fileReadable(mangled))
            return mangled;
        const std::string mangledVersioned = mangled + ";1";
        if (PlatformStorage::fileReadable(mangledVersioned))
            return mangledVersioned;
    }
    return std::string();
}

// Directories are checked in the opposite order: opendir() on this driver
// returns a valid handle for a path whose case does not match what the disc
// records, and that handle then enumerates empty. Ask for the recorded
// spelling first so a false positive on the caller's spelling cannot win.
std::string resolveDiscDirectory(const std::string& path)
{
    const std::string mangled = iso9660Path(path);
    if (mangled != path && PlatformStorage::isDirectory(mangled))
        return mangled;
    if (PlatformStorage::isDirectory(path))
        return path;
    return std::string();
}

bool isDisabledPath(const std::string& path)
{
    const std::string normalized = PlatformStorage::normalizeSlashes(path);
    // Treat the disabled backend as a device prefix, not as a filesystem path.
    // Older callers can concatenate a world name directly ("nosave:World")
    // while newer ones insert a slash ("nosave:/World"). Neither form must
    // ever fall through to newlib, where it would become an unknown device.
    return normalized.compare(0, 7, "nosave:") == 0;
}

bool mkdirs(const std::string& path)
{
    const std::string normalized = PlatformStorage::normalizeSlashes(path);
    if (isDisabledPath(normalized))
        return true;
    if (isMemoryCardPath(normalized))
        return Ps2MemoryCardFileSystem::mkdirs(normalized);
    return PlatformStorage::makeDirectories(normalized);
}

bool writeFile(const std::string& path, const void* data, std::size_t length)
{
    const std::string normalized = PlatformStorage::normalizeSlashes(path);
    if (isDisabledPath(normalized))
        return true;
    if (isMemoryCardPath(normalized))
        return Ps2MemoryCardFileSystem::writeFile(normalized, data, length);
    return PlatformStorage::posixWriteFile(normalized, data, length);
}

bool appendFile(const std::string& path, const void* data, std::size_t length)
{
    const std::string normalized = PlatformStorage::normalizeSlashes(path);
    if (isDisabledPath(normalized))
        return true;
    if (isMemoryCardPath(normalized))
        return Ps2MemoryCardFileSystem::appendFile(normalized, data, length);
    return PlatformStorage::posixAppendFile(normalized, data, length);
}

bool readFile(const std::string& path, std::vector<unsigned char>& out)
{
    const std::string normalized = PlatformStorage::normalizeSlashes(path);
    if (isDisabledPath(normalized))
    {
        out.clear();
        return false;
    }
    if (isMemoryCardPath(normalized))
        return Ps2MemoryCardFileSystem::readFile(normalized, out);
    if (isDiscPath(normalized))
    {
        const std::string resolved = resolveDiscFile(normalized);
        if (resolved.empty())
        {
            out.clear();
            return false;
        }
        return PlatformStorage::posixReadFile(resolved, out);
    }
    return PlatformStorage::posixReadFile(normalized, out);
}

std::int64_t getFileSize(const std::string& path)
{
    const std::string normalized = PlatformStorage::normalizeSlashes(path);
    if (isDisabledPath(normalized))
        return -1;
    if (isMemoryCardPath(normalized))
        return Ps2MemoryCardFileSystem::getFileSize(normalized);
    if (isDiscPath(normalized))
    {
        const std::string resolved = resolveDiscFile(normalized);
        return resolved.empty() ? -1 : PlatformStorage::fileSize(resolved);
    }
    return PlatformStorage::fileSize(normalized);
}

bool readFileRange(const std::string& path, std::size_t offset, void* out, std::size_t length)
{
    const std::string normalized = PlatformStorage::normalizeSlashes(path);
    if (isDisabledPath(normalized) || (out == nullptr && length != 0))
        return false;
    if (length == 0)
        return true;
    if (isMemoryCardPath(normalized))
        return Ps2MemoryCardFileSystem::readFileRange(normalized, offset, out, length);

    if (offset > static_cast<std::size_t>(std::numeric_limits<long>::max()))
        return false;

    std::string source = normalized;
    if (isDiscPath(normalized))
    {
        source = resolveDiscFile(normalized);
        if (source.empty())
            return false;
    }

    FILE* input = std::fopen(source.c_str(), "rb");
    if (input == nullptr)
        return false;
    if (std::fseek(input, static_cast<long>(offset), SEEK_SET) != 0)
    {
        std::fclose(input);
        return false;
    }

    const std::size_t count = std::fread(out, 1, length, input);
    std::fclose(input);
    return count == length;
}

bool exists(const std::string& path)
{
    const std::string normalized = PlatformStorage::normalizeSlashes(path);
    if (isDisabledPath(normalized))
        return false;
    if (isMemoryCardPath(normalized))
        return Ps2MemoryCardFileSystem::exists(normalized);
    if (isDiscPath(normalized))
        return !resolveDiscFile(normalized).empty() || !resolveDiscDirectory(normalized).empty();
    return PlatformStorage::posixExists(normalized);
}

bool removeFile(const std::string& path)
{
    const std::string normalized = PlatformStorage::normalizeSlashes(path);
    if (isDisabledPath(normalized))
        return true;
    if (isMemoryCardPath(normalized))
        return Ps2MemoryCardFileSystem::removeFile(normalized);
    return PlatformStorage::removePath(normalized);
}

bool isDirectory(const std::string& path)
{
    const std::string normalized = PlatformStorage::normalizeSlashes(path);
    if (isDisabledPath(normalized))
        return false;
    // Only data/resources is in the manifest. Anything else on the disc --
    // data/assets/legacy/tutorial, for one -- still has to go through POSIX,
    // where opendir() answers existence correctly even though enumerating the
    // handle does not work.
    if (isDiscPath(normalized))
    {
        if (Ps2ResourceManifest::covers(normalized))
            return Ps2ResourceManifest::isDirectory(normalized);
        return !resolveDiscDirectory(normalized).empty();
    }
    return PlatformStorage::isDirectory(normalized);
}

bool listDirs(const std::string& path, std::vector<std::string>& out)
{
    const std::string normalized = PlatformStorage::normalizeSlashes(path);
    if (isDisabledPath(normalized))
    {
        out.clear();
        return true;
    }
    if (isMemoryCardPath(normalized))
        return Ps2MemoryCardFileSystem::listEntries(normalized, out);
    if (isDiscPath(normalized))
    {
        if (Ps2ResourceManifest::covers(normalized))
            return Ps2ResourceManifest::listChildren(normalized, out);
        // Outside the manifest, enumeration is still the unreliable operation
        // this driver has -- resolve the recorded spelling and let the POSIX
        // glue answer, which is what the callers here already tolerate.
        const std::string resolved = resolveDiscDirectory(normalized);
        return PlatformStorage::listEntries(resolved.empty() ? normalized : resolved, out);
    }
    return PlatformStorage::listEntries(normalized, out);
}

} // namespace Ps2SaveFileSystem

#endif // PS2_PLATFORM
