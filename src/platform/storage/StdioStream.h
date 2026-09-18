#pragma once

#include <istream>
#include <memory>
#include <ostream>
#include <string>

namespace PlatformStorage
{
std::unique_ptr<std::istream> openStdioInputStream(const std::string& path);
std::unique_ptr<std::ostream> openStdioOutputStream(const std::string& path, bool append = false);
std::unique_ptr<std::ostream> openDiscardOutputStream();
}
