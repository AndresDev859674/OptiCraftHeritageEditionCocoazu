#pragma once

#include <string>
#include <vector>

#include "java/Type.h"

struct LegacyLanguageEntry
{
    std::string code;
    std::string displayName;
};

class LegacyLanguageModel
{
public:
    LegacyLanguageModel();
    LegacyLanguageModel(const std::vector<LegacyLanguageEntry> &entries, int_t pageSize);

    void reset(const std::vector<LegacyLanguageEntry> &entries, int_t pageSize);
    int_t pageSize() const;
    int_t pageCount() const;
    int_t clampPage(int_t page) const;
    const LegacyLanguageEntry *entryAt(int_t page, int_t row) const;
    int_t pageContaining(const std::string &code) const;

private:
    std::vector<LegacyLanguageEntry> languageEntries;
    int_t entriesPerPage;
};
