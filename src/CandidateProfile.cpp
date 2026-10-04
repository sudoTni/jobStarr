#include "CandidateProfile.h"
#include <QRegularExpression>
#include <QStringList>
#include <QMap>
#include <algorithm>

namespace jobstarr {

bool CandidateProfile::isValid(QString *errorMessage) const {
    if (professionalTitle.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Candidate Professional Title is empty.");
        return false;
    }
    if (professionalSummary.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Candidate Professional Summary is empty.");
        return false;
    }
    if (keySkills.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Candidate Key Skills is empty.");
        return false;
    }
    if (resume.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Candidate Resume is empty.");
        return false;
    }
    // Testimonials may legitimately be empty (Section 7)
    return true;
}

QString CandidateProfile::normalizeKeySkills(const QString &rawSkills) {
    if (rawSkills.trimmed().isEmpty()) {
        return QString();
    }

    static const QRegularExpression lineSplitter(QStringLiteral("[\r\n]+"));
    static const QRegularExpression bulletRegex(QStringLiteral("^\\s*[-*+•]\\s*"));

    const QStringList lines = rawSkills.split(lineSplitter, Qt::SkipEmptyParts);
    QStringList rawTokens;

    for (QString line : lines) {
        line = line.trimmed();
        line.remove(bulletRegex);
        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const QStringList parts = line.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (QString part : parts) {
            part = part.trimmed();
            part.remove(bulletRegex);
            part = part.trimmed();
            if (!part.isEmpty()) {
                rawTokens.append(part);
            }
        }
    }

    // De-duplicate case-insensitively while preserving title/uppercase casing if present
    QMap<QString, QString> uniqueMap;
    for (const QString &token : rawTokens) {
        const QString lower = token.toLower();
        if (!uniqueMap.contains(lower)) {
            uniqueMap.insert(lower, token);
        } else {
            const QString existing = uniqueMap.value(lower);
            if (!existing.isEmpty() && existing[0].isLower() && !token.isEmpty() && token[0].isUpper()) {
                uniqueMap.insert(lower, token);
            }
        }
    }

    QStringList resultList = uniqueMap.values();
    std::sort(resultList.begin(), resultList.end(), [](const QString &a, const QString &b) {
        int cmp = a.compare(b, Qt::CaseInsensitive);
        if (cmp != 0) {
            return cmp < 0;
        }
        return a < b;
    });

    return resultList.join(QStringLiteral(", "));
}

} // namespace jobstarr
