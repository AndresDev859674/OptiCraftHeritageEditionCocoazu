#pragma once

namespace LegacyStartup
{

enum class PresentationMode
{
    Java,
    Legacy
};

inline PresentationMode selectPresentation(bool legacyUi)
{
    return legacyUi ? PresentationMode::Legacy : PresentationMode::Java;
}

} // namespace LegacyStartup
