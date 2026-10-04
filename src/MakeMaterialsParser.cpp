#include "MakeMaterialsParser.h"
#include <QRegularExpression>
#include <QStringList>

namespace jobstarr {

const QString MakeMaterialsParser::kHeadingResumeFilename = QStringLiteral("Resume Filename");
const QString MakeMaterialsParser::kHeadingCoverLetterFilename = QStringLiteral("Cover Letter Filename");
const QString MakeMaterialsParser::kHeadingTitle = QStringLiteral("Optimized & Tailored Professional Title");
const QString MakeMaterialsParser::kHeadingSummary = QStringLiteral("Optimized & Tailored Professional Summary");
const QString MakeMaterialsParser::kHeadingSkills = QStringLiteral("Optimized & Tailored Key Skills");
const QString MakeMaterialsParser::kHeadingCoverLetter = QStringLiteral("Optimized & Tailored Cover Letter");

int MakeMaterialsParser::countParagraphs(const QString &text) {
    const QStringList lines = text.split(QLatin1Char('\n'));
    int count = 0;
    bool inParagraph = false;

    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) {
            inParagraph = false;
        } else {
            if (!inParagraph) {
                count++;
                inParagraph = true;
            }
        }
    }
    return count;
}

int MakeMaterialsParser::countWords(const QString &text) {
    static const QRegularExpression whitespaceRegex(QStringLiteral("\\s+"));
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return 0;
    }
    const QStringList words = trimmed.split(whitespaceRegex, Qt::SkipEmptyParts);
    return words.size();
}

std::optional<MakeMaterialsResult> MakeMaterialsParser::parse(const QString &rawResponse, QString *errorMessage) {
    if (rawResponse.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Response is empty.");
        return std::nullopt;
    }

    // Normalize line endings to \n
    QString normalized = rawResponse;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    const QStringList lines = normalized.split(QLatin1Char('\n'));

    const QStringList expectedHeadings = {
        kHeadingResumeFilename,
        kHeadingCoverLetterFilename,
        kHeadingTitle,
        kHeadingSummary,
        kHeadingSkills,
        kHeadingCoverLetter
    };

    QSet<QString> recognizedSet(expectedHeadings.begin(), expectedHeadings.end());
    QSet<QString> seenHeadings;

    int currentExpectedIdx = 0;
    int currentSectionIdx = -1;
    QStringList sectionContents;
    sectionContents.resize(expectedHeadings.size());

    static const QRegularExpression headingRegex(QStringLiteral("^#\\s+(.*)$"));

    for (const QString &line : lines) {
        const QString trimmedLine = line.trimmed();

        const QRegularExpressionMatch match = headingRegex.match(trimmedLine);
        if (match.hasMatch()) {
            const QString headingTitle = match.captured(1).trimmed();

            if (recognizedSet.contains(headingTitle)) {
                if (seenHeadings.contains(headingTitle)) {
                    if (errorMessage) {
                        *errorMessage = QStringLiteral("Duplicated recognized heading: '# %1'").arg(headingTitle);
                    }
                    return std::nullopt;
                }

                if (currentExpectedIdx >= expectedHeadings.size() ||
                    expectedHeadings[currentExpectedIdx] != headingTitle) {
                    if (errorMessage) {
                        const QString expected = (currentExpectedIdx < expectedHeadings.size())
                                                     ? expectedHeadings[currentExpectedIdx]
                                                     : QStringLiteral("<end of sections>");
                        *errorMessage = QStringLiteral("Headings out of expected order: expected '# %1', got '# %2'")
                                            .arg(expected, headingTitle);
                    }
                    return std::nullopt;
                }

                seenHeadings.insert(headingTitle);
                currentSectionIdx = currentExpectedIdx;
                currentExpectedIdx++;
                continue;
            } else {
                // An unrecognized line formatted as an H1 heading '# ...'
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Unknown extra heading: '%1'").arg(trimmedLine);
                }
                return std::nullopt;
            }
        }

        if (currentSectionIdx == -1) {
            // Content before the first recognized heading
            if (!trimmedLine.isEmpty()) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Unknown extra content before first heading: '%1'").arg(trimmedLine);
                }
                return std::nullopt;
            }
        } else {
            // Append line to current section
            if (!sectionContents[currentSectionIdx].isEmpty()) {
                sectionContents[currentSectionIdx].append(QLatin1Char('\n'));
            }
            sectionContents[currentSectionIdx].append(line);
        }
    }

    // Verify all expected headings were encountered
    if (currentExpectedIdx < expectedHeadings.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Missing required section: '%1'").arg(expectedHeadings[currentExpectedIdx]);
        }
        return std::nullopt;
    }

    MakeMaterialsResult result;
    result.resumeFilename = sectionContents[0].trimmed();
    result.coverLetterFilename = sectionContents[1].trimmed();
    result.professionalTitle = sectionContents[2].trimmed();
    result.professionalSummary = sectionContents[3].trimmed();
    result.optimizedSkills = sectionContents[4].trimmed();
    result.coverLetterBody = sectionContents[5].trimmed();

    // Check for empty sections
    for (int i = 0; i < expectedHeadings.size(); ++i) {
        if (sectionContents[i].trimmed().isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Section '%1' is empty.").arg(expectedHeadings[i]);
            }
            return std::nullopt;
        }
    }

    // Paragraph count validation
    result.coverLetterParagraphCount = countParagraphs(result.coverLetterBody);
    if (result.coverLetterParagraphCount > 4) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cover letter exceeds maximum 4 paragraphs: found %1 paragraphs.")
                                .arg(result.coverLetterParagraphCount);
        }
        return std::nullopt;
    }

    // Word count calculation & diagnostic warning
    result.coverLetterWordCount = countWords(result.coverLetterBody);
    if (result.coverLetterWordCount < 240 || result.coverLetterWordCount > 280) {
        result.warnings.append(
            QStringLiteral("Cover letter word count (%1) is outside recommended 240–280 word range.")
                .arg(result.coverLetterWordCount));
    }

    return result;
}

} // namespace jobstarr
