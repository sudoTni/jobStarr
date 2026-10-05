// Portions derived from JobSpy (https://github.com/speedyapply/JobSpy)
// Copyright (c) 2023 Cullen Watson
// Licensed under the MIT License (see third_party_licenses/LICENSE.JobSpy)

#include "LinkedInScraper.h"
#include "JobParsingUtilities.h"
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <libxml/xmlsave.h>
#include <QUrl>
#include <QUrlQuery>

namespace jobstarr {

static QString evaluateXPathString(xmlXPathContextPtr ctx, const char *xpath) {
    xmlXPathObjectPtr obj = xmlXPathEvalExpression((const xmlChar *)xpath, ctx);
    if (!obj) return QString();
    QString result;
    if (obj->nodesetval && obj->nodesetval->nodeNr > 0) {
        xmlChar *content = xmlNodeGetContent(obj->nodesetval->nodeTab[0]);
        if (content) {
            result = QString::fromUtf8((const char *)content).trimmed();
            xmlFree(content);
        }
    }
    xmlXPathFreeObject(obj);
    return result;
}

static QString evaluateXPathAttribute(xmlXPathContextPtr ctx, const char *xpath, const char *attrName) {
    xmlXPathObjectPtr obj = xmlXPathEvalExpression((const xmlChar *)xpath, ctx);
    if (!obj) return QString();
    QString result;
    if (obj->nodesetval && obj->nodesetval->nodeNr > 0) {
        xmlChar *prop = xmlGetProp(obj->nodesetval->nodeTab[0], (const xmlChar *)attrName);
        if (prop) {
            result = QString::fromUtf8((const char *)prop).trimmed();
            xmlFree(prop);
        }
    }
    xmlXPathFreeObject(obj);
    return result;
}

static QString evaluateXPathInnerHtml(xmlDocPtr doc, xmlXPathContextPtr ctx, const char *xpath) {
    xmlXPathObjectPtr obj = xmlXPathEvalExpression((const xmlChar *)xpath, ctx);
    if (!obj) return QString();
    QString result;
    if (obj->nodesetval && obj->nodesetval->nodeNr > 0) {
        xmlNodePtr node = obj->nodesetval->nodeTab[0];
        xmlBufferPtr buf = xmlBufferCreate();
        for (xmlNodePtr child = node->children; child; child = child->next) {
            xmlNodeDump(buf, doc, child, 0, 1);
        }
        result = QString::fromUtf8((const char *)xmlBufferContent(buf)).trimmed();
        xmlBufferFree(buf);
    }
    xmlXPathFreeObject(obj);
    return result;
}

LinkedInScraper::LinkedInScraper(QNetworkAccessManager *nam, QObject *parent)
    : JobScraper(parent),
      m_nam(nam ? nam : new QNetworkAccessManager(this)),
      m_ownsNam(nam == nullptr),
      m_timer(new QTimer(this)) {
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &LinkedInScraper::onTimeout);
}

LinkedInScraper::~LinkedInScraper() {
    cancel();
}

bool LinkedInScraper::isRunning() const {
    return m_currentReply != nullptr && m_currentReply->isRunning();
}

void LinkedInScraper::cancel() {
    m_timer->stop();
    if (m_currentReply) {
        auto *reply = m_currentReply;
        m_currentReply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
}

void LinkedInScraper::scrape(const QString &url, int timeoutMs) {
    if (isRunning()) {
        emit error(QStringLiteral("A job grabbing operation is already in progress."));
        return;
    }

    const QString jobId = JobParsingUtilities::extractLinkedInJobId(url);
    if (jobId.isEmpty()) {
        emit error(QStringLiteral("Unable to extract LinkedIn job ID from URL: %1").arg(url));
        return;
    }

    m_currentJobId = jobId;
    m_timedOut = false;

    const QUrl requestUrl(QStringLiteral("https://www.linkedin.com/jobs/view/%1").arg(jobId));
    QNetworkRequest request(requestUrl);

    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"));
    request.setRawHeader("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.9");
    request.setRawHeader("Cache-Control", "max-age=0");
    request.setRawHeader("Upgrade-Insecure-Requests", "1");
    request.setRawHeader("authority", "www.linkedin.com");

    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    emit started();

    m_timer->start(timeoutMs);
    m_currentReply = m_nam->get(request);
    connect(m_currentReply, &QNetworkReply::finished, this, &LinkedInScraper::onReplyFinished);
}

void LinkedInScraper::onTimeout() {
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

void LinkedInScraper::onReplyFinished() {
    m_timer->stop();
    if (m_timedOut) return;

    if (!m_currentReply) return;

    const auto networkError = m_currentReply->error();
    const int statusCode = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QUrl redirectUrl = m_currentReply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();

    // Check for authwall / redirect to login
    if (redirectUrl.toString().contains(QLatin1String("linkedin.com/signup")) ||
        redirectUrl.toString().contains(QLatin1String("linkedin.com/login"))) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        emit error(QStringLiteral("LinkedIn redirected to sign-in page. The posting may require login or has expired."));
        return;
    }

    if (networkError != QNetworkReply::NoError && statusCode == 0) {
        const QString errStr = m_currentReply->errorString();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        emit error(QStringLiteral("Network error fetching LinkedIn job: %1").arg(errStr));
        return;
    }

    if (statusCode == 404) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        emit error(QStringLiteral("LinkedIn job not found (HTTP 404)."));
        return;
    }

    if (statusCode != 200 && statusCode != 0) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        emit error(QStringLiteral("LinkedIn scrape failed with HTTP status %1.").arg(statusCode));
        return;
    }

    const QByteArray responseData = m_currentReply->readAll();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    const QString html = QString::fromUtf8(responseData);
    QString parseErr;
    auto recordOpt = parseHtml(html, m_currentJobId, &parseErr);
    if (!recordOpt.has_value()) {
        emit error(QStringLiteral("Failed to parse LinkedIn job posting: %1").arg(parseErr));
        return;
    }

    emit success(recordOpt.value());
}

std::optional<JobRecord> LinkedInScraper::parseHtml(const QString &html, const QString &jobId, QString *errorMessage) {
    if (html.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("HTML content is empty.");
        return std::nullopt;
    }

    const QByteArray htmlUtf8 = html.toUtf8();
    htmlDocPtr doc = htmlReadMemory(
        htmlUtf8.constData(),
        htmlUtf8.size(),
        nullptr,
        "UTF-8",
        HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING
    );

    if (!doc) {
        if (errorMessage) *errorMessage = QStringLiteral("Failed to parse HTML document.");
        return std::nullopt;
    }

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (!ctx) {
        xmlFreeDoc(doc);
        if (errorMessage) *errorMessage = QStringLiteral("Failed to create XPath context.");
        return std::nullopt;
    }

    // 1. Title
    QString title = evaluateXPathString(ctx, "//h1[contains(@class, 'top-card-layout__title')]");
    if (title.isEmpty()) {
        title = evaluateXPathString(ctx, "//h2[contains(@class, 'top-card-layout__title')]");
    }
    if (title.isEmpty()) {
        title = evaluateXPathString(ctx, "//span[contains(@class, 'sr-only')]");
    }
    if (title.isEmpty()) {
        title = QStringLiteral("N/A");
    }

    // 2. Company Name & URL
    QString company = evaluateXPathString(ctx, "//a[contains(@class, 'topcard__org-name-link')]");
    QString companyUrl = evaluateXPathAttribute(ctx, "//a[contains(@class, 'topcard__org-name-link')]", "href");
    if (company.isEmpty()) {
        company = evaluateXPathString(ctx, "//h4[contains(@class, 'base-search-card__subtitle')]//a");
        companyUrl = evaluateXPathAttribute(ctx, "//h4[contains(@class, 'base-search-card__subtitle')]//a", "href");
    }
    if (company.isEmpty()) {
        company = evaluateXPathString(ctx, "//h4[contains(@class, 'base-search-card__subtitle')]");
    }
    if (!companyUrl.isEmpty()) {
        QUrl cUrl(companyUrl);
        cUrl.setQuery(QString());
        companyUrl = cUrl.toString();
    }

    // 3. Location
    QString locStr = evaluateXPathString(ctx, "//span[contains(@class, 'topcard__flavor--bullet')]");
    if (locStr.isEmpty()) {
        locStr = evaluateXPathString(ctx, "//span[contains(@class, 'job-search-card__location')]");
    }

    QJsonObject locObj;
    const QStringList parts = locStr.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.size() == 1) {
        locObj[QStringLiteral("city")] = parts.at(0).trimmed();
        locObj[QStringLiteral("state")] = QJsonValue(QJsonValue::Null);
        locObj[QStringLiteral("country")] = QStringLiteral("USA");
    } else if (parts.size() == 2) {
        locObj[QStringLiteral("city")] = parts.at(0).trimmed();
        locObj[QStringLiteral("state")] = parts.at(1).trimmed();
        locObj[QStringLiteral("country")] = QStringLiteral("USA");
    } else if (parts.size() >= 3) {
        locObj[QStringLiteral("city")] = parts.at(0).trimmed();
        locObj[QStringLiteral("state")] = parts.at(1).trimmed();
        locObj[QStringLiteral("country")] = parts.at(2).trimmed();
    } else {
        locObj[QStringLiteral("city")] = QJsonValue(QJsonValue::Null);
        locObj[QStringLiteral("state")] = QJsonValue(QJsonValue::Null);
        locObj[QStringLiteral("country")] = QStringLiteral("USA");
    }

    // 4. Date Posted
    QString datePosted = evaluateXPathAttribute(ctx, "//time", "datetime");
    if (datePosted.isEmpty()) {
        const QString timeAgoText = evaluateXPathString(ctx, "//span[contains(@class, 'posted-time-ago__text')]");
        datePosted = JobParsingUtilities::parseRelativeDate(timeAgoText);
    }

    // 5. Description & Emails
    const QString descInnerHtml = evaluateXPathInnerHtml(doc, ctx, "//div[contains(@class, 'show-more-less-html__markup')]");
    const QString description = JobParsingUtilities::htmlToMarkdown(descInnerHtml);
    const QStringList emails = JobParsingUtilities::extractEmails(description);

    // 6. Job Type
    const QString jobTypeCriteria = evaluateXPathString(
        ctx,
        "//h3[contains(@class, 'description__job-criteria-subheader') and contains(., 'Employment type')]/following-sibling::span[contains(@class, 'description__job-criteria-text')]"
    );
    QJsonArray jobTypeArray;
    if (!jobTypeCriteria.isEmpty()) {
        const QString mappedType = JobParsingUtilities::mapJobTypeString(jobTypeCriteria);
        if (!mappedType.isEmpty()) {
            jobTypeArray.append(mappedType);
        }
    }

    // 7. Job Level
    QString jobLevel = evaluateXPathString(
        ctx,
        "//h3[contains(@class, 'description__job-criteria-subheader') and contains(., 'Seniority level')]/following-sibling::span[contains(@class, 'description__job-criteria-text')]"
    );
    if (!jobLevel.isEmpty()) {
        jobLevel = jobLevel.toLower();
    }

    // 8. Job Function
    const QString jobFunction = evaluateXPathString(
        ctx,
        "//h3[contains(., 'Job function')]/following::span[contains(@class, 'description__job-criteria-text')]"
    );

    // 9. Industry
    const QString companyIndustry = evaluateXPathString(
        ctx,
        "//h3[contains(@class, 'description__job-criteria-subheader') and contains(., 'Industries')]/following-sibling::span[contains(@class, 'description__job-criteria-text')]"
    );

    // 10. Company Logo
    const QString companyLogo = evaluateXPathAttribute(ctx, "//img[contains(@class, 'artdeco-entity-image')]", "data-delayed-url");

    // 11. Compensation
    const QString salaryText = evaluateXPathString(ctx, "//div[contains(@class, 'compensation__salary')]");
    const auto compOpt = JobParsingUtilities::parseLinkedInCompensation(salaryText);

    // 12. Remote
    const bool isRemote = JobParsingUtilities::isJobRemote(title, locStr);

    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);

    // Assemble final normalized JSON object
    QJsonObject root;
    root[QStringLiteral("source")] = QStringLiteral("linkedin");
    root[QStringLiteral("id")] = QStringLiteral("li-%1").arg(jobId);
    root[QStringLiteral("title")] = title;
    root[QStringLiteral("company_name")] = company.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(company);
    root[QStringLiteral("company_url")] = companyUrl.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(companyUrl);
    root[QStringLiteral("company_url_direct")] = QJsonValue(QJsonValue::Null);
    root[QStringLiteral("location")] = locObj;
    root[QStringLiteral("date_posted")] = datePosted.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(datePosted);
    root[QStringLiteral("job_url")] = QStringLiteral("https://www.linkedin.com/jobs/view/%1").arg(jobId);
    root[QStringLiteral("job_url_direct")] = QJsonValue(QJsonValue::Null);
    root[QStringLiteral("is_remote")] = isRemote;
    root[QStringLiteral("job_type")] = jobTypeArray.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(jobTypeArray);
    root[QStringLiteral("job_level")] = jobLevel.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(jobLevel);
    root[QStringLiteral("job_function")] = jobFunction.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(jobFunction);
    root[QStringLiteral("listing_type")] = QJsonValue(QJsonValue::Null);
    root[QStringLiteral("compensation")] = compOpt.has_value() ? QJsonValue(compOpt.value()) : QJsonValue(QJsonValue::Null);
    root[QStringLiteral("description")] = description.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(description);

    QJsonArray emailsArr;
    for (const auto &em : emails) emailsArr.append(em);
    root[QStringLiteral("emails")] = emailsArr;

    root[QStringLiteral("company_industry")] = companyIndustry.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(companyIndustry);
    root[QStringLiteral("company_addresses")] = QJsonValue(QJsonValue::Null);
    root[QStringLiteral("company_num_employees")] = QJsonValue(QJsonValue::Null);
    root[QStringLiteral("company_revenue")] = QJsonValue(QJsonValue::Null);
    root[QStringLiteral("company_description")] = QJsonValue(QJsonValue::Null);
    root[QStringLiteral("company_logo")] = companyLogo.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(companyLogo);

    return JobRecord::fromJsonObject(root, errorMessage);
}

} // namespace jobstarr
