#pragma once

#include <QString>
#include <QStringList>
#include <QDate>
#include <QJsonObject>
#include <QJsonArray>
#include <optional>
#include <utility>

namespace jobstarr {

class JobParsingUtilities {
public:
    static QStringList extractEmails(const QString &text);
    static QString htmlToMarkdown(const QString &html);
    static QString parseRelativeDate(const QString &text, const QDate &baseDate = QDate::currentDate());
    static bool isJobRemote(const QString &title, const QString &locationStr);
    static bool isIndeedJobRemote(const QJsonArray &attributes, const QString &formattedLocation);
    static QString extractLinkedInJobId(const QString &url);
    static QString extractIndeedJobKey(const QString &url);
    static std::pair<QString, QString> indeedCountryAndSubdomain(const QString &host);
    static std::optional<QJsonObject> parseLinkedInCompensation(const QString &salaryText);
    static std::optional<QJsonObject> parseIndeedCompensation(const QJsonObject &compObj);
    static QString mapJobTypeString(const QString &str);
    static QStringList mapIndeedJobTypeAttributes(const QJsonArray &attributes);
};

} // namespace jobstarr
