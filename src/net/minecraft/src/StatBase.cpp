#include "StatBase.h"

#ifdef PS2_PLATFORM
#include "util/CompactNumberFormat.h"
#else
#include <sstream>
#include <iomanip>
#include <locale>
#endif
#include "IStatType.h"
#include "StatList.h"
#include "AchievementMap.h"
#include "StatTypeSimple.h"
#include "StatTypeTime.h"
#include "StatTypeDistance.h"
#include <stdexcept>

StatBase::StatBase(int_t i, const std::string &s, IStatType *istattype) :
	statId(i),
	statName(s),
	independent(false),
	statType(istattype)
{
}

StatBase::StatBase(int_t i, const std::string &s) :
	StatBase(i, s, simpleStatType)
{
}

StatBase* StatBase::setIndependent()
{
	independent = true;
	return this;
}

StatBase* StatBase::registerStat()
{
	auto it = StatList::statMap.find(statId);
	if (it != StatList::statMap.end())
	{
		const std::string message = "Duplicate stat id: " + it->second->statName +
			" and " + statName + " at id " + std::to_string(statId);
		throw std::runtime_error(message);
	}

	StatList::allStats.push_back(this);
	StatList::statMap[statId] = this;
	statGuid = AchievementMap::getGuid(statId);
	return this;
}

bool StatBase::isAchievement()
{
	return false;
}

std::string StatBase::format(int_t i)
{
	return statType->format(i);
}

std::string StatBase::toString()
{
	return statName;
}

std::string StatBase::numberFormat(int_t i)
{
#ifdef PS2_PLATFORM
	return CompactNumberFormat::integer(i);
#else
	std::ostringstream oss;
	oss.imbue(std::locale(""));
	oss << i;
	return oss.str();
#endif
}

std::string StatBase::decimalFormat(double d)
{
#ifdef PS2_PLATFORM
	return CompactNumberFormat::fixed2(d);
#else
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(2) << d;
	return oss.str();
#endif
}

IStatType *StatBase::simpleStatType = new StatTypeSimple();
IStatType *StatBase::timeStatType = new StatTypeTime();
IStatType *StatBase::distanceStatType = new StatTypeDistance();
