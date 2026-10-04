#include "UrlValidator.h"
#include <QRegularExpression>

namespace jobstarr {

static bool isLinkedInHost(const QString &host) {
    static const QRegularExpression linkedinRegex(
        QStringLiteral(R"(^(?:[a-zA-Z0-9-]+\.)*linkedin\.com$)"),
        QRegularExpression::CaseInsensitiveOption
    );
    return linkedinRegex.match(host).hasMatch();
}

static bool isIndeedHost(const QString &host) {
    static const QRegularExpression indeedRegex(
        QStringLiteral(R"(^(?:[a-zA-Z0-9-]+\.)*indeed\.(?:com|[a-zA-Z]{2,3}(?:\.[a-zA-Z]{2})?)$)"),
        QRegularExpression::CaseInsensitiveOption
    );
    return indeedRegex.match(host).hasMatch();
}

bool UrlValidator::isValidJobUrl(const QString &urlString, QString *errorMessage) {
    const QString trimmed = urlString.trimmed();
    if (trimmed.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Job URL cannot be empty.");
        return false;
    }

    QUrl url(trimmed);
    if (!url.isValid()) {
        if (errorMessage) *errorMessage = QStringLiteral("Invalid URL format.");
        return false;
    }

    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https")) {
        if (errorMessage) *errorMessage = QStringLiteral("URL must use http or https protocol.");
        return false;
    }

    const QString host = url.host().toLower();
    if (host.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("URL host is missing.");
        return false;
    }

    if (isLinkedInHost(host) || isIndeedHost(host)) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("Unsupported host '%1'. Only LinkedIn and Indeed URLs are supported.").arg(host);
    }
    return false;
}

JobSource UrlValidator::detectSource(const QString &urlString) {
    QUrl url(urlString.trimmed());
    if (!url.isValid()) return JobSource::Unknown;

    const QString host = url.host().toLower();
    if (isLinkedInHost(host)) return JobSource::LinkedIn;
    if (isIndeedHost(host)) return JobSource::Indeed;

    return JobSource::Unknown;
}

QString UrlValidator::sourceToString(JobSource source) {
    switch (source) {
        case JobSource::LinkedIn: return QStringLiteral("linkedin");
        case JobSource::Indeed:   return QStringLiteral("indeed");
        default:                  return QStringLiteral("unknown");
    }
}

} // namespace jobstarr
