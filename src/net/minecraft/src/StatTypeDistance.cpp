#include "StatTypeDistance.h"

#ifdef PS2_PLATFORM
#include "util/CompactNumberFormat.h"
#else
#include <sstream>
#include <iomanip>
#endif

StatTypeDistance::StatTypeDistance()
{
}

std::string StatTypeDistance::format(int_t i)
{
	int_t j = i;
	double d = (double)j / 100.0;
	double d1 = d / 1000.0;
	if (d1 > 0.5)
	{
#ifdef PS2_PLATFORM
		return CompactNumberFormat::fixed2(d1) + " km";
#else
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(2) << d1 << " km";
		return oss.str();
#endif
	}
	if (d > 0.5)
	{
#ifdef PS2_PLATFORM
		return CompactNumberFormat::fixed2(d) + " m";
#else
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(2) << d << " m";
		return oss.str();
#endif
	}
	return std::to_string(i) + " cm";
}
