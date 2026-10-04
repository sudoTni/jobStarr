#pragma once

#include <QString>

namespace jobstarr {

struct CandidateProfile {
    QString professionalTitle;
    QString professionalSummary;
    QString keySkills;
    QString resume;
    QString testimonials; // Optional per Section 7

    bool isValid(QString *errorMessage = nullptr) const;
    static QString normalizeKeySkills(const QString &rawSkills);
};

} // namespace jobstarr
