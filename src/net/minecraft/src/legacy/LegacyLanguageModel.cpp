#include "LegacyLanguageModel.h"

#include <algorithm>

LegacyLanguageModel::LegacyLanguageModel()
    : entriesPerPage(1)
{
}

LegacyLanguageModel::LegacyLanguageModel(const std::vector<LegacyLanguageEntry> &entries, int_t pageSizeValue)
    : entriesPerPage(1)
{
    reset(entries, pageSizeValue);
}

void LegacyLanguageModel::reset(const std::vector<LegacyLanguageEntry> &entries, int_t pageSizeValue)
{
    languageEntries = entries;
    entriesPerPage = std::max<int_t>(1, pageSizeValue);
}

int_t LegacyLanguageModel::pageSize() const
{
    return entriesPerPage;
}

int_t LegacyLanguageModel::pageCount() const
{
    if (languageEntries.empty())
        return 1;
    return static_cast<int_t>((languageEntries.size() + static_cast<size_t>(entriesPerPage) - 1) /
        static_cast<size_t>(entriesPerPage));
}

int_t LegacyLanguageModel::clampPage(int_t page) const
{
    return std::max<int_t>(0, std::min<int_t>(page, pageCount() - 1));
}

const LegacyLanguageEntry *LegacyLanguageModel::entryAt(int_t page, int_t row) const
{
    if (row < 0 || row >= entriesPerPage)
        return nullptr;
    const int_t safePage = clampPage(page);
    const size_t index = static_cast<size_t>(safePage * entriesPerPage + row);
    return index < languageEntries.size() ? &languageEntries[index] : nullptr;
}

int_t LegacyLanguageModel::pageContaining(const std::string &code) const
{
    for (size_t i = 0; i < languageEntries.size(); ++i)
    {
        if (languageEntries[i].code == code)
            return static_cast<int_t>(i / static_cast<size_t>(entriesPerPage));
    }
    return 0;
}
