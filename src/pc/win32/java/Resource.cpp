#include "java/Resource.h"

#include <algorithm>
#include <fstream>
#include <cassert>
#include <memory>
#include <stdexcept>

#include "java/File.h"
#include "net/minecraft/src/GameResources.h"

#include "util/Memory.h"

static const File &ResourceFile()
{
	static std::unique_ptr<File> resource_file(File::openResourceDirectory());
	return *resource_file;
}

namespace Resource
{

std::istream *getResource(const jstring &name)
{
	// assets.pak beside the exe first (AssetPak); the resource directory is
	// the loose-file fallback it always was.
	std::unique_ptr<std::istream> packed = GameResources::open(static_cast<const std::string &>(name));
	if (packed)
		return packed.release();

	const File &resource_file = ResourceFile();
	std::unique_ptr<File> file(File::open(resource_file, name));
	std::unique_ptr<std::istream> is(file->toStreamIn());
	if (!is)
		throw std::runtime_error("Failed to open resource " + name);
	return is.release();
}

}
