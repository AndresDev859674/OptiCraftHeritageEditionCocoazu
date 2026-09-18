#pragma once

// Build-time replacement for a live cdrom0: directory listing -- see the
// comment at the top of cmake/ps2_resource_manifest.cmake for why. Backs
// PlatformStorage::listPathEntries()/pathIsDirectory() (via
// Ps2SaveFileSystem) for any disc path under Ps2Assets::resourcesDir().

#include <string>
#include <vector>

namespace Ps2ResourceManifest
{

// True if discPath lies under the resources root, i.e. if the two queries
// below can answer for it at all. Callers must check this first and fall back
// to their normal handling when it is false: a disc path outside the resources
// tree (data/assets/legacy/tutorial, say) is not in the manifest, and treating
// the resulting "no" as an answer reports real directories as missing.
bool covers(const std::string& discPath);

// True if discPath is the resources root, or a directory beneath it that
// the manifest lists at least one file under. False for a leaf file, and
// false for any path outside the resources root (callers should fall back
// to their normal handling in that case).
bool isDirectory(const std::string& discPath);

// Fills out with the immediate child names (files and subdirectories,
// unqualified, matching manifest casing) of discPath. Returns false only
// when discPath falls outside the resources root; a real, childless
// directory returns true with out left empty.
bool listChildren(const std::string& discPath, std::vector<std::string>& out);

} // namespace Ps2ResourceManifest
