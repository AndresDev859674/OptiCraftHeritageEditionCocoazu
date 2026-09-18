#pragma once

#include <string>

// net.minecraft.src.StatCollector
class StatCollector
{
public:
	static std::string translateToLocal(const std::string &s);
	static std::string translateToLocalFormatted(const std::string &s, const std::string &arg);
};
