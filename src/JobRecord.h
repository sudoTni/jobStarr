#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonDocument>
#include <optional>

namespace jobstarr {

class JobRecord {
public:
    JobRecord() = default;

    static std::optional<JobRecord> fromJson(const QString &jsonString, QString *errorMessage = nullptr);
    static std::optional<JobRecord> fromJsonObject(const QJsonObject &jsonObject, QString *errorMessage = nullptr);

    QString toJsonString(bool pretty = true) const;
    const QJsonObject &jsonObject() const { return m_jsonObject; }

    bool isValid() const { return !m_id.isEmpty() && !m_title.isEmpty(); }

    QString source() const { return m_source; }
    QString id() const { return m_id; }
    QString title() const { return m_title; }
    QString companyName() const { return m_companyName; }
    QString companyUrl() const { return m_companyUrl; }
    QString jobUrl() const { return m_jobUrl; }
    QString datePosted() const { return m_datePosted; }
    QString description() const { return m_description; }
    bool isRemote() const { return m_isRemote; }
    QString location() const;
    QString companyUrlDirect() const { return m_jsonObject.value(QLatin1String("company_url_direct")).toString(); }
    QStringList emails() const { return m_emails; }

private:
    QJsonObject m_jsonObject;
    QString m_source;
    QString m_id;
    QString m_title;
    QString m_companyName;
    QString m_companyUrl;
    QString m_jobUrl;
    QString m_datePosted;
    QString m_description;
    bool m_isRemote{false};
    QStringList m_emails;
};

} // namespace jobstarr
