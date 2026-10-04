#pragma once

#include <QString>
#include <QStringList>

namespace jobstarr {

struct MakeMaterialsResult {
    QString resumeFilename;
    QString coverLetterFilename;

    QString professionalTitle;
    QString professionalSummary;
    QString optimizedSkills;

    QString coverLetterBody;

    int coverLetterWordCount{0};
    int coverLetterParagraphCount{0};
    QStringList warnings;

    bool isValid(QString *errorMessage = nullptr) const {
        if (resumeFilename.trimmed().isEmpty()) {
            if (errorMessage) *errorMessage = QStringLiteral("Resume Filename is empty.");
            return false;
        }
        if (coverLetterFilename.trimmed().isEmpty()) {
            if (errorMessage) *errorMessage = QStringLiteral("Cover Letter Filename is empty.");
            return false;
        }
        if (professionalTitle.trimmed().isEmpty()) {
            if (errorMessage) *errorMessage = QStringLiteral("Optimized Professional Title is empty.");
            return false;
        }
        if (professionalSummary.trimmed().isEmpty()) {
            if (errorMessage) *errorMessage = QStringLiteral("Optimized Professional Summary is empty.");
            return false;
        }
        if (optimizedSkills.trimmed().isEmpty()) {
            if (errorMessage) *errorMessage = QStringLiteral("Optimized Key Skills is empty.");
            return false;
        }
        if (coverLetterBody.trimmed().isEmpty()) {
            if (errorMessage) *errorMessage = QStringLiteral("Optimized Cover Letter is empty.");
            return false;
        }
        if (coverLetterParagraphCount > 4) {
            if (errorMessage) *errorMessage = QStringLiteral("Cover letter exceeds maximum 4 paragraphs (found %1).")
                                                  .arg(coverLetterParagraphCount);
            return false;
        }
        return true;
    }
};

} // namespace jobstarr
