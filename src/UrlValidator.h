#pragma once

#include <QString>
#include <QUrl>

namespace jobstarr {

enum class JobSource {
    Unknown,
    LinkedIn,
    Indeed
};

class UrlValidator {
public:
    static bool isValidJobUrl(const QString &urlString, QString *errorMessage = nullptr);
    static JobSource detectSource(const QString &urlString);
    static QString sourceToString(JobSource source);
};

} // namespace jobstarr
