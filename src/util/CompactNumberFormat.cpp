#include "util/CompactNumberFormat.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace
{
template <typename T>
bool sameBits(T left, T right)
{
	unsigned char leftBytes[sizeof(T)];
	unsigned char rightBytes[sizeof(T)];
	std::memcpy(leftBytes, &left, sizeof(T));
	std::memcpy(rightBytes, &right, sizeof(T));
	return std::memcmp(leftBytes, rightBytes, sizeof(T)) == 0;
}

bool parseExponent(const std::string& text, std::size_t position, int& value)
{
	bool negative = false;
	if (position < text.size() && (text[position] == '+' || text[position] == '-'))
	{
		negative = text[position] == '-';
		++position;
	}
	if (position >= text.size())
		return false;

	int result = 0;
	for (; position < text.size(); ++position)
	{
		const char c = text[position];
		if (c < '0' || c > '9')
			return false;
		if (result < 1000000)
			result = result * 10 + (c - '0');
	}
	value = negative ? -result : result;
	return true;
}

std::string normalizeJavaDecimal(const std::string& token, bool negative)
{
	std::string value = token;
	if (!value.empty() && (value[0] == '+' || value[0] == '-'))
		value.erase(value.begin());

	int exponentPart = 0;
	const std::size_t exponentPosition = value.find_first_of("eE");
	if (exponentPosition != std::string::npos)
	{
		if (!parseExponent(value, exponentPosition + 1, exponentPart))
			exponentPart = 0;
		value.resize(exponentPosition);
	}

	const std::size_t dot = value.find('.');
	const int digitsBeforeDot = dot == std::string::npos
		? static_cast<int>(value.size())
		: static_cast<int>(dot);
	std::string digits = value;
	if (dot != std::string::npos)
		digits.erase(dot, 1);

	const std::size_t first = digits.find_first_not_of('0');
	if (first == std::string::npos)
		return negative ? "-0.0" : "0.0";

	const int scientificExponent = exponentPart + digitsBeforeDot - 1 - static_cast<int>(first);
	digits.erase(0, first);
	while (digits.size() > 1 && digits.back() == '0')
		digits.pop_back();

	std::string output;
	if (negative)
		output.push_back('-');
	if (scientificExponent >= -3 && scientificExponent < 7)
	{
		const int point = scientificExponent + 1;
		if (point <= 0)
		{
			output += "0.";
			output.append(static_cast<std::size_t>(-point), '0');
			output += digits;
		}
		else if (point >= static_cast<int>(digits.size()))
		{
			output += digits;
			output.append(static_cast<std::size_t>(point - static_cast<int>(digits.size())), '0');
			output += ".0";
		}
		else
		{
			output.append(digits, 0, static_cast<std::size_t>(point));
			output.push_back('.');
			output.append(digits, static_cast<std::size_t>(point), std::string::npos);
		}
	}
	else
	{
		output.push_back(digits[0]);
		output.push_back('.');
		if (digits.size() == 1)
			output.push_back('0');
		else
			output.append(digits, 1, std::string::npos);
		output.push_back('E');
		output += CompactNumberFormat::integer(scientificExponent);
	}
	return output;
}

bool parseExact(const std::string& text, float& value)
{
	char* end = nullptr;
	value = std::strtof(text.c_str(), &end);
	return end != text.c_str() && end != nullptr && *end == '\0';
}

bool parseExact(const std::string& text, double& value)
{
	char* end = nullptr;
	value = std::strtod(text.c_str(), &end);
	return end != text.c_str() && end != nullptr && *end == '\0';
}

template <typename T>
std::string javaFloatingPoint(T value)
{
	if (std::isnan(value))
		return "NaN";
	if (std::isinf(value))
		return std::signbit(value) ? "-Infinity" : "Infinity";
	if (value == static_cast<T>(0))
		return std::signbit(value) ? "-0.0" : "0.0";

	const bool negative = std::signbit(value);
	const T magnitude = negative ? -value : value;
	char buffer[96];
	std::string best;
	for (int precision = 1; precision <= std::numeric_limits<T>::max_digits10; ++precision)
	{
		const int length = std::snprintf(buffer, sizeof(buffer), "%.*g", precision, static_cast<double>(magnitude));
		if (length <= 0 || static_cast<std::size_t>(length) >= sizeof(buffer))
			continue;

		const std::string candidate(buffer, static_cast<std::size_t>(length));
		T parsed{};
		if (parseExact(candidate, parsed) && sameBits(parsed, magnitude))
		{
			best = candidate;
			break;
		}
	}
	if (best.empty())
	{
		const int length = std::snprintf(buffer, sizeof(buffer), "%.*g",
			std::numeric_limits<T>::max_digits10, static_cast<double>(magnitude));
		if (length > 0 && static_cast<std::size_t>(length) < sizeof(buffer))
			best.assign(buffer, static_cast<std::size_t>(length));
	}
	return normalizeJavaDecimal(best, negative);
}
}

namespace CompactNumberFormat
{
std::string javaFloat(float value)
{
	return javaFloatingPoint(value);
}

std::string javaDouble(double value)
{
	return javaFloatingPoint(value);
}

std::string fixed2(double value)
{
	char buffer[64];
	const int length = std::snprintf(buffer, sizeof(buffer), "%.2f", value);
	if (length <= 0 || static_cast<std::size_t>(length) >= sizeof(buffer))
		return std::string();
	return std::string(buffer, static_cast<std::size_t>(length));
}

std::string integer(long long value)
{
	char buffer[32];
	const int length = std::snprintf(buffer, sizeof(buffer), "%lld", value);
	if (length <= 0 || static_cast<std::size_t>(length) >= sizeof(buffer))
		return std::string();
	return std::string(buffer, static_cast<std::size_t>(length));
}

bool parseFloatExact(const std::string& text, float& value)
{
	return parseExact(text, value);
}
}
