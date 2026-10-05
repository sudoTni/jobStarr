// Portions derived from JobSpy (https://github.com/speedyapply/JobSpy)
// Copyright (c) 2023 Cullen Watson
// Licensed under the MIT License (see third_party_licenses/LICENSE.JobSpy)

#include "IndeedScraper.h"
#include "JobParsingUtilities.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTimeZone>
#include <QUrl>

namespace jobstarr {

static QString formatTitleCase(const QString &input) {
    if (input.isEmpty()) return QString();
    const QStringList words = input.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList result;
    for (const QString &w : words) {
        if (w.isEmpty()) continue;
        result.append(w.at(0).toUpper() + w.mid(1).toLower());
    }
    return result.join(QLatin1Char(' '));
}

IndeedScraper::IndeedScraper(QNetworkAccessManager *nam, QObject *parent)
    : JobScraper(parent),
      m_nam(nam ? nam : new QNetworkAccessManager(this)),
      m_ownsNam(nam == nullptr),
      m_timer(new QTimer(this)) {
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &IndeedScraper::onTimeout);
}

IndeedScraper::~IndeedScraper() {
    cancel();
}

bool IndeedScraper::isRunning() const {
    return m_currentReply != nullptr && m_currentReply->isRunning();
}

void IndeedScraper::cancel() {
    m_timer->stop();
    if (m_currentReply) {
        auto *reply = m_currentReply;
        m_currentReply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
}

void IndeedScraper::scrape(const QString &url, int timeoutMs) {
    if (isRunning()) {
        emit error(QStringLiteral("A job grabbing operation is already in progress."));
        return;
    }

    const QString jobKey = JobParsingUtilities::extractIndeedJobKey(url);
    if (jobKey.isEmpty()) {
        emit error(QStringLiteral("Unable to extract Indeed job key from URL: %1").arg(url));
        return;
    }

    m_currentJobKey = jobKey;
    m_timedOut = false;

    const QUrl parsedUrl(url.trimmed());
    const auto countryInfo = JobParsingUtilities::indeedCountryAndSubdomain(parsedUrl.host());
    const QString subdomain = countryInfo.first;
    const QString countryCode = countryInfo.second;

    m_currentBaseUrl = QStringLiteral("https://%1.indeed.com").arg(subdomain);

    const QUrl endpoint(QStringLiteral("https://apis.indeed.com/graphql"));
    QNetworkRequest request(endpoint);

    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Host", "apis.indeed.com");
    request.setRawHeader("indeed-api-key", "161092c2017b5bbab13edb12461a62d5a833871e7cad6d9d475304573de67ac8");
    request.setRawHeader("accept", "application/json");
    request.setRawHeader("accept-language", "en-US,en;q=0.9");
    request.setRawHeader("user-agent", "Mozilla/5.0 (iPhone; CPU iPhone OS 16_6_1 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Mobile/15E148 Indeed App 193.1");
    request.setRawHeader("indeed-app-info", "appv=193.1; appid=com.indeed.jobsearch; osv=16.6.1; os=ios; dtype=phone");
    request.setRawHeader("indeed-co", countryCode.toLatin1());
    request.setRawHeader("indeed-locale", "en-US");

    static const QString queryTemplate = QStringLiteral(
        "query {\n"
        "    jobData(jobKeys: [\"%1\"]) {\n"
        "        results {\n"
        "            job {\n"
        "                source { name }\n"
        "                key\n"
        "                title\n"
        "                datePublished\n"
        "                dateOnIndeed\n"
        "                description { html }\n"
        "                location {\n"
        "                    countryName countryCode admin1Code city postalCode streetAddress\n"
        "                    formatted { short long }\n"
        "                }\n"
        "                compensation {\n"
        "                    estimated { currencyCode baseSalary { unitOfWork range { ... on Range { min max } } } }\n"
        "                    baseSalary { unitOfWork range { ... on Range { min max } } }\n"
        "                    currencyCode\n"
        "                }\n"
        "                attributes { key label }\n"
        "                employer {\n"
        "                    relativeCompanyPageUrl name\n"
        "                    dossier {\n"
        "                        employerDetails { addresses industry employeesLocalizedLabel revenueLocalizedLabel briefDescription ceoName ceoPhotoUrl }\n"
        "                        images { headerImageUrl squareLogoUrl }\n"
        "                        links { corporateWebsite }\n"
        "                    }\n"
        "                }\n"
        "                recruit { viewJobUrl detailedSalary workSchedule }\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}\n"
    );

    QJsonObject payload;
    payload[QStringLiteral("query")] = queryTemplate.arg(jobKey);

    const QByteArray requestData = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    emit started();

    m_timer->start(timeoutMs);
    m_currentReply = m_nam->post(request, requestData);
    connect(m_currentReply, &QNetworkReply::finished, this, &IndeedScraper::onReplyFinished);
}

void IndeedScraper::onTimeout() {
    if (isRunning()) {
        m_timedOut = true;
        if (m_currentReply) {
            auto *reply = m_currentReply;
            m_currentReply = nullptr;
            reply->disconnect(this);
            reply->abort();
            reply->deleteLater();
        }
        emit error(QStringLiteral("Job grabbing operation timed out."));
    }
}

void IndeedScraper::onReplyFinished() {
    m_timer->stop();
    if (m_timedOut) return;

    if (!m_currentReply) return;

    const auto networkError = m_currentReply->error();
    const int statusCode = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (networkError != QNetworkReply::NoError && statusCode == 0) {
        const QString errStr = m_currentReply->errorString();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        emit error(QStringLiteral("Network error fetching Indeed job: %1").arg(errStr));
        return;
    }

    if (statusCode != 200 && statusCode != 0) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        emit error(QStringLiteral("Indeed scrape failed with HTTP status %1.").arg(statusCode));
        return;
    }

    const QByteArray responseData = m_currentReply->readAll();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    QString parseErr;
    auto recordOpt = parseJson(responseData, m_currentJobKey, m_currentBaseUrl, &parseErr);
    if (!recordOpt.has_value()) {
        emit error(parseErr);
        return;
    }

    emit success(recordOpt.value());
}

std::optional<JobRecord> IndeedScraper::parseJson(const QByteArray &jsonBytes,
                                                 const QString &jobKey,
                                                 const QString &baseUrl,
                                                 QString *errorMessage) {
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to parse Indeed GraphQL response: %1").arg(parseErr.errorString());
        }
        return std::nullopt;
    }

    const QJsonObject root = doc.object();
    const QJsonObject data = root.value(QLatin1String("data")).toObject();
    const QJsonObject jobData = data.value(QLatin1String("jobData")).toObject();
    const QJsonArray results = jobData.value(QLatin1String("results")).toArray();

    if (results.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Indeed job not found for key: %1").arg(jobKey);
        }
        return std::nullopt;
    }

    const QJsonObject firstResult = results.at(0).toObject();
    if (!firstResult.contains(QLatin1String("job")) || !firstResult.value(QLatin1String("job")).isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Indeed job not found for key: %1").arg(jobKey);
        }
        return std::nullopt;
    }

    const QJsonObject job = firstResult.value(QLatin1String("job")).toObject();

    // 1. Basic identifiers
    const QString key = job.value(QLatin1String("key")).toString(jobKey);
    const QString id = QStringLiteral("in-%1").arg(key);
    const QString title = job.value(QLatin1String("title")).toString();

    // 2. Description
    const QString descHtml = job.value(QLatin1String("description")).toObject().value(QLatin1String("html")).toString();
    const QString description = JobParsingUtilities::htmlToMarkdown(descHtml);
    const QStringList emails = JobParsingUtilities::extractEmails(description);

    // 3. Location
    const QJsonObject locData = job.value(QLatin1String("location")).toObject();
    QJsonObject locObj;
    locObj[QStringLiteral("city")] = locData.value(QLatin1String("city")).toString();
    locObj[QStringLiteral("state")] = locData.value(QLatin1String("admin1Code")).toString();
    locObj[QStringLiteral("country")] = locData.value(QLatin1String("countryCode")).toString().toUpper();

    // 4. Date Posted
    QString datePosted;
    if (job.contains(QLatin1String("datePublished")) && !job.value(QLatin1String("datePublished")).isNull()) {
        const qint64 epochMs = job.value(QLatin1String("datePublished")).toVariant().toLongLong();
        if (epochMs > 0) {
            datePosted = QDateTime::fromMSecsSinceEpoch(epochMs, QTimeZone::UTC).toString(QStringLiteral("yyyy-MM-dd"));
        }
    }

    // 5. Attributes & Job Type & Remote
    const QJsonArray attributes = job.value(QLatin1String("attributes")).toArray();
    const QString formattedLong = locData.value(QLatin1String("formatted")).toObject().value(QLatin1String("long")).toString();

    const bool isRemote = JobParsingUtilities::isIndeedJobRemote(attributes, formattedLong);
    const QStringList jobTypes = JobParsingUtilities::mapIndeedJobTypeAttributes(attributes);
    QJsonArray jobTypeArr;
    for (const auto &jt : jobTypes) jobTypeArr.append(jt);

    // 6. Compensation
    const auto compOpt = JobParsingUtilities::parseIndeedCompensation(job.value(QLatin1String("compensation")).toObject());

    // 7. Employer metadata
    const QJsonObject employer = job.value(QLatin1String("employer")).toObject();
    const QString companyName = employer.value(QLatin1String("name")).toString();
    const QString relUrl = employer.value(QLatin1String("relativeCompanyPageUrl")).toString();
    const QString companyUrl = relUrl.isEmpty() ? QString() : (baseUrl + relUrl);

    QString companyUrlDirect;
    QString companyAddresses;
    QString companyIndustry;
    QString companyNumEmployees;
    QString companyRevenue;
    QString companyDescription;
    QString companyLogo;

    if (employer.contains(QLatin1String("dossier")) && employer.value(QLatin1String("dossier")).isObject()) {
        const QJsonObject dossier = employer.value(QLatin1String("dossier")).toObject();

        if (dossier.contains(QLatin1String("links")) && dossier.value(QLatin1String("links")).isObject()) {
            companyUrlDirect = dossier.value(QLatin1String("links")).toObject().value(QLatin1String("corporateWebsite")).toString();
        }

        if (dossier.contains(QLatin1String("employerDetails")) && dossier.value(QLatin1String("employerDetails")).isObject()) {
            const QJsonObject details = dossier.value(QLatin1String("employerDetails")).toObject();

            const QJsonArray addrs = details.value(QLatin1String("addresses")).toArray();
            if (!addrs.isEmpty()) {
                companyAddresses = addrs.at(0).toString();
            }

            QString rawInd = details.value(QLatin1String("industry")).toString();
            if (!rawInd.isEmpty()) {
                rawInd.remove(QStringLiteral("Iv1"));
                rawInd.replace(QLatin1Char('_'), QLatin1Char(' '));
                companyIndustry = formatTitleCase(rawInd.trimmed());
            }

            companyNumEmployees = details.value(QLatin1String("employeesLocalizedLabel")).toString();
            companyRevenue = details.value(QLatin1String("revenueLocalizedLabel")).toString();
            companyDescription = details.value(QLatin1String("briefDescription")).toString();
        }

        if (dossier.contains(QLatin1String("images")) && dossier.value(QLatin1String("images")).isObject()) {
            companyLogo = dossier.value(QLatin1String("images")).toObject().value(QLatin1String("squareLogoUrl")).toString();
        }
    }

    // 8. Recruit metadata
    QString jobUrlDirect;
    if (job.contains(QLatin1String("recruit")) && job.value(QLatin1String("recruit")).isObject()) {
        jobUrlDirect = job.value(QLatin1String("recruit")).toObject().value(QLatin1String("viewJobUrl")).toString();
    }

    // Assemble final normalized JSON object matching JobSpy contract
    QJsonObject recordObj;
    recordObj[QStringLiteral("source")] = QStringLiteral("indeed");
    recordObj[QStringLiteral("id")] = id;
    recordObj[QStringLiteral("title")] = title;
    recordObj[QStringLiteral("company_name")] = companyName.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(companyName);
    recordObj[QStringLiteral("company_url")] = companyUrl.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(companyUrl);
    recordObj[QStringLiteral("company_url_direct")] = companyUrlDirect.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(companyUrlDirect);
    recordObj[QStringLiteral("location")] = locObj;
    recordObj[QStringLiteral("date_posted")] = datePosted.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(datePosted);
    recordObj[QStringLiteral("job_url")] = QStringLiteral("%1/viewjob?jk=%2").arg(baseUrl, key);
    recordObj[QStringLiteral("job_url_direct")] = jobUrlDirect.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(jobUrlDirect);
    recordObj[QStringLiteral("is_remote")] = isRemote;
    recordObj[QStringLiteral("job_type")] = jobTypeArr.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(jobTypeArr);
    recordObj[QStringLiteral("job_level")] = QJsonValue(QJsonValue::Null);
    recordObj[QStringLiteral("job_function")] = QJsonValue(QJsonValue::Null);
    recordObj[QStringLiteral("listing_type")] = QJsonValue(QJsonValue::Null);
    recordObj[QStringLiteral("compensation")] = compOpt.has_value() ? QJsonValue(compOpt.value()) : QJsonValue(QJsonValue::Null);
    recordObj[QStringLiteral("description")] = description.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(description);

    QJsonArray emailArr;
    for (const auto &e : emails) emailArr.append(e);
    recordObj[QStringLiteral("emails")] = emailArr;

    recordObj[QStringLiteral("company_industry")] = companyIndustry.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(companyIndustry);
    recordObj[QStringLiteral("company_addresses")] = companyAddresses.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(companyAddresses);
    recordObj[QStringLiteral("company_num_employees")] = companyNumEmployees.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(companyNumEmployees);
    recordObj[QStringLiteral("company_revenue")] = companyRevenue.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(companyRevenue);
    recordObj[QStringLiteral("company_description")] = companyDescription.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(companyDescription);
    recordObj[QStringLiteral("company_logo")] = companyLogo.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(companyLogo);

    return JobRecord::fromJsonObject(recordObj, errorMessage);
}

} // namespace jobstarr
