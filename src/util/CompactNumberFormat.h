#pragma once

#include <string>

namespace CompactNumberFormat
{
std::string javaFloat(float value);
std::string javaDouble(double value);
std::string fixed2(double value);
std::string integer(long long value);
bool parseFloatExact(const std::string& text, float& value);
}
