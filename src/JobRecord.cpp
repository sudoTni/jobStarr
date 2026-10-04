#include "JobRecord.h"
#include <QJsonArray>
#include <QJsonParseError>

namespace jobstarr {

std::optional<JobRecord> JobRecord::fromJson(const QString &jsonString, QString *errorMessage) {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("JSON parse error: %1 (offset: %2)")
                                .arg(parseError.errorString())
                                .arg(parseError.offset);
        }
        return std::nullopt;
    }

    if (!doc.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Expected top-level JSON object for JobRecord.");
        }
        return std::nullopt;
    }

    return fromJsonObject(doc.object(), errorMessage);
}

std::optional<JobRecord> JobRecord::fromJsonObject(const QJsonObject &obj, QString *errorMessage) {
    JobRecord record;
    record.m_jsonObject = obj;

    record.m_source = obj.value(QLatin1String("source")).toString();
    record.m_id = obj.value(QLatin1String("id")).toString();
    record.m_title = obj.value(QLatin1String("title")).toString();
    record.m_companyName = obj.value(QLatin1String("company_name")).toString();
    record.m_companyUrl = obj.value(QLatin1String("company_url")).toString();
    record.m_jobUrl = obj.value(QLatin1String("job_url")).toString();
    record.m_datePosted = obj.value(QLatin1String("date_posted")).toString();
    record.m_description = obj.value(QLatin1String("description")).toString();
    record.m_isRemote = obj.value(QLatin1String("is_remote")).toBool(false);

    const QJsonValue emailsVal = obj.value(QLatin1String("emails"));
    if (emailsVal.isArray()) {
        for (const auto &item : emailsVal.toArray()) {
            if (item.isString()) {
                record.m_emails.append(item.toString());
            }
        }
    }

    if (record.m_id.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Missing required 'id' in job record.");
        return std::nullopt;
    }
    if (record.m_title.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Missing required 'title' in job record.");
        return std::nullopt;
    }

    return record;
}

QString JobRecord::toJsonString(bool pretty) const {
    const QJsonDocument doc(m_jsonObject);
    return QString::fromUtf8(doc.toJson(pretty ? QJsonDocument::Indented : QJsonDocument::Compact));
}

QString JobRecord::location() const {
    const QJsonValue locVal = m_jsonObject.value(QLatin1String("location"));
    if (locVal.isString()) {
        return locVal.toString().trimmed();
    }
    if (locVal.isObject()) {
        const QJsonObject locObj = locVal.toObject();
        QStringList parts;
        const QString city = locObj.value(QLatin1String("city")).toString().trimmed();
        const QString state = locObj.value(QLatin1String("state")).toString().trimmed();
        const QString country = locObj.value(QLatin1String("country")).toString().trimmed();
        if (!city.isEmpty()) parts.append(city);
        if (!state.isEmpty()) parts.append(state);
        if (!country.isEmpty()) parts.append(country);
        return parts.join(QStringLiteral(", "));
    }
    return QString();
}

} // namespace jobstarr
