#pragma once

#include <QString>
#include <optional>
#include "MakeMaterialsResult.h"

namespace jobstarr {

class MakeMaterialsParser {
public:
    static std::optional<MakeMaterialsResult> parse(const QString &rawResponse, QString *errorMessage = nullptr);

    static int countParagraphs(const QString &text);
    static int countWords(const QString &text);

    static const QString kHeadingResumeFilename;
    static const QString kHeadingCoverLetterFilename;
    static const QString kHeadingTitle;
    static const QString kHeadingSummary;
    static const QString kHeadingSkills;
    static const QString kHeadingCoverLetter;
};

} // namespace jobstarr
