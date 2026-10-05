// Portions derived from JobSpy (https://github.com/speedyapply/JobSpy)
// Copyright (c) 2023 Cullen Watson
// Licensed under the MIT License (see third_party_licenses/LICENSE.JobSpy)

#include "JobParsingUtilities.h"
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <QTextDocument>
#include <cmath>

namespace jobstarr {

QStringList JobParsingUtilities::extractEmails(const QString &text) {
    QStringList result;
    if (text.isEmpty()) return result;

    static const QRegularExpression emailRegex(
        QStringLiteral(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})")
    );

    auto it = emailRegex.globalMatch(text);
    while (it.hasNext()) {
        auto match = it.next();
        const QString email = match.captured(0);
        if (!result.contains(email)) {
            result.append(email);
        }
    }
    return result;
}

QString JobParsingUtilities::htmlToMarkdown(const QString &html) {
    if (html.isEmpty()) return QString();
    QTextDocument doc;
    doc.setHtml(html);
    return doc.toMarkdown().trimmed();
}

QString JobParsingUtilities::parseRelativeDate(const QString &text, const QDate &baseDate) {
    if (text.isEmpty()) return QString();

    static const QRegularExpression relRegex(
        QStringLiteral(R"((\d+)\s+(day|week|month|year|hour|minute)s?\s+ago)"),
        QRegularExpression::CaseInsensitiveOption
    );

    auto match = relRegex.match(text);
    if (!match.hasMatch()) return QString();

    const int val = match.captured(1).toInt();
    const QString unit = match.captured(2).toLower();

    QDate targetDate = baseDate;
    if (unit == QLatin1String("hour") || unit == QLatin1String("minute")) {
        targetDate = baseDate;
    } else if (unit == QLatin1String("day")) {
        targetDate = baseDate.addDays(-val);
    } else if (unit == QLatin1String("week")) {
        targetDate = baseDate.addDays(-val * 7);
    } else if (unit == QLatin1String("month")) {
        targetDate = baseDate.addDays(-val * 30);
    } else if (unit == QLatin1String("year")) {
        targetDate = baseDate.addDays(-val * 365);
    }

    return targetDate.toString(QStringLiteral("yyyy-MM-dd"));
}

bool JobParsingUtilities::isJobRemote(const QString &title, const QString &locationStr) {
    const QString full = QStringLiteral("%1 %2").arg(title, locationStr).toLower();
    return full.contains(QLatin1String("remote")) ||
           full.contains(QLatin1String("work from home")) ||
           full.contains(QLatin1String("wfh"));
}

bool JobParsingUtilities::isIndeedJobRemote(const QJsonArray &attributes, const QString &formattedLocation) {
    static const QStringList remoteKeys = {
        QStringLiteral("DSQF7"),
        QStringLiteral("N83EH"),
        QStringLiteral("5STP8")
    };

    for (const auto &val : attributes) {
        if (!val.isObject()) continue;
        const QJsonObject obj = val.toObject();
        const QString key = obj.value(QLatin1String("key")).toString();
        if (remoteKeys.contains(key)) {
            return true;
        }
        const QString label = obj.value(QLatin1String("label")).toString().toLower();
        if (label.contains(QLatin1String("remote")) ||
            label.contains(QLatin1String("work from home")) ||
            label.contains(QLatin1String("wfh"))) {
            return true;
        }
    }

    const QString locLower = formattedLocation.toLower();
    if (locLower.contains(QLatin1String("remote")) ||
        locLower.contains(QLatin1String("work from home")) ||
        locLower.contains(QLatin1String("wfh"))) {
        return true;
    }

    return false;
}

QString JobParsingUtilities::extractLinkedInJobId(const QString &url) {
    const QUrl parsed(url.trimmed());

    // 1. Query parameter: currentJobId
    const QUrlQuery query(parsed);
    if (query.hasQueryItem(QStringLiteral("currentJobId"))) {
        const QString qid = query.queryItemValue(QStringLiteral("currentJobId")).trimmed();
        if (!qid.isEmpty()) return qid;
    }

    // 2. Path: /jobs/view/<slug>-(\d{6,}) or /jobs/view/(\d{6,})
    static const QRegularExpression pathViewRegex(
        QStringLiteral(R"(/jobs/view/(?:[a-zA-Z0-9_-]+-)?(\d{6,}))")
    );
    auto match = pathViewRegex.match(parsed.path());
    if (match.hasMatch()) {
        return match.captured(1);
    }

    // 3. Fallback: Any sequence of 6+ digits in path
    static const QRegularExpression digitsRegex(QStringLiteral(R"((\d{6,}))"));
    auto m2 = digitsRegex.match(parsed.path());
    if (m2.hasMatch()) {
        return m2.captured(1);
    }

    return QString();
}

QString JobParsingUtilities::extractIndeedJobKey(const QString &url) {
    const QUrl parsed(url.trimmed());

    // 1. Query parameters
    const QUrlQuery query(parsed);
    for (const QString &param : {QStringLiteral("jk"), QStringLiteral("vjk"), QStringLiteral("jobkey")}) {
        if (query.hasQueryItem(param)) {
            const QString val = query.queryItemValue(param).trimmed();
            if (!val.isEmpty()) return val;
        }
    }

    // 2. Regex in raw URL
    static const QRegularExpression qRegex(QStringLiteral(R"([?&](?:jk|vjk)=([a-zA-Z0-9_-]+))"));
    auto match = qRegex.match(url);
    if (match.hasMatch()) {
        return match.captured(1);
    }

    // 3. Path ending with 16+ hex/alphanumeric chars
    const QStringList parts = parsed.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (!parts.isEmpty()) {
        static const QRegularExpression hexRegex(QStringLiteral(R"(^([a-zA-Z0-9]{16,})$)"));
        auto hm = hexRegex.match(parts.last());
        if (hm.hasMatch()) {
            return hm.captured(1);
        }
    }

    return QString();
}

std::pair<QString, QString> JobParsingUtilities::indeedCountryAndSubdomain(const QString &host) {
    const QString h = host.toLower();

    // Mapping table: {subdomain, countryCode}
    static const struct CountryMapping {
        const char *domainKeyword;
        const char *subdomain;
        const char *countryCode;
    } mappings[] = {
        {"ar.indeed", "ar", "AR"},
        {"au.indeed", "au", "AU"},
        {"at.indeed", "at", "AT"},
        {"be.indeed", "be", "BE"},
        {"br.indeed", "br", "BR"},
        {"ca.indeed", "ca", "CA"},
        {"ch.indeed", "ch", "CH"},
        {"cl.indeed", "cl", "CL"},
        {"co.indeed", "co", "CO"},
        {"cr.indeed", "cr", "CR"},
        {"cz.indeed", "cz", "CZ"},
        {"de.indeed", "de", "DE"},
        {"dk.indeed", "dk", "DK"},
        {"ec.indeed", "ec", "EC"},
        {"es.indeed", "es", "ES"},
        {"fi.indeed", "fi", "FI"},
        {"fr.indeed", "fr", "FR"},
        {"gr.indeed", "gr", "GR"},
        {"hk.indeed", "hk", "HK"},
        {"hu.indeed", "hu", "HU"},
        {"id.indeed", "id", "ID"},
        {"ie.indeed", "ie", "IE"},
        {"in.indeed", "in", "IN"},
        {"it.indeed", "it", "IT"},
        {"jp.indeed", "jp", "JP"},
        {"lu.indeed", "lu", "LU"},
        {"mx.indeed", "mx", "MX"},
        {"my.indeed", "malaysia", "MY"},
        {"nl.indeed", "nl", "NL"},
        {"no.indeed", "no", "NO"},
        {"nz.indeed", "nz", "NZ"},
        {"pe.indeed", "pe", "PE"},
        {"ph.indeed", "ph", "PH"},
        {"pl.indeed", "pl", "PL"},
        {"pt.indeed", "pt", "PT"},
        {"ro.indeed", "ro", "RO"},
        {"se.indeed", "se", "SE"},
        {"sg.indeed", "sg", "SG"},
        {"tr.indeed", "tr", "TR"},
        {"tw.indeed", "tw", "TW"},
        {"ua.indeed", "ua", "UA"},
        {"uk.indeed", "uk", "GB"},
        {"vn.indeed", "vn", "VN"},
        {"za.indeed", "za", "ZA"}
    };

    for (const auto &m : mappings) {
        if (h.contains(QLatin1String(m.domainKeyword))) {
            return {QString::fromLatin1(m.subdomain), QString::fromLatin1(m.countryCode)};
        }
    }

    // Default to USA
    return {QStringLiteral("www"), QStringLiteral("US")};
}

std::optional<QJsonObject> JobParsingUtilities::parseLinkedInCompensation(const QString &salaryText) {
    if (salaryText.isEmpty()) return std::nullopt;

    // e.g., "$117,000.00/yr - $234,000.00/yr" or "$50.00/hr - $75.00/hr" or "$120,000/yr"
    static const QRegularExpression rangeRegex(
        QStringLiteral(R"(([^\d\s]+)\s*([\d,]+(?:\.\d+)?)(/\w+)?\s*[-—–]\s*[^\d\s]*\s*([\d,]+(?:\.\d+)?)(/\w+)?)")
    );

    static const QRegularExpression singleRegex(
        QStringLiteral(R"(([^\d\s]+)\s*([\d,]+(?:\.\d+)?)(/\w+)?)")
    );

    QString rawCurrency = QStringLiteral("USD");
    double minAmount = 0.0;
    double maxAmount = 0.0;
    QString interval = QStringLiteral("yearly");

    auto rangeMatch = rangeRegex.match(salaryText);
    if (rangeMatch.hasMatch()) {
        const QString curr = rangeMatch.captured(1).trimmed();
        const QString minStr = rangeMatch.captured(2).remove(QLatin1Char(','));
        const QString interval1 = rangeMatch.captured(3).trimmed();
        const QString maxStr = rangeMatch.captured(4).remove(QLatin1Char(','));
        const QString interval2 = rangeMatch.captured(5).trimmed();

        minAmount = minStr.toDouble();
        maxAmount = maxStr.toDouble();

        const QString intervalStr = interval2.isEmpty() ? interval1 : interval2;
        if (intervalStr == QLatin1String("/hr")) interval = QStringLiteral("hourly");
        else if (intervalStr == QLatin1String("/mo")) interval = QStringLiteral("monthly");
        else if (intervalStr == QLatin1String("/yr")) interval = QStringLiteral("yearly");

        if (curr == QStringLiteral("$")) rawCurrency = QStringLiteral("USD");
        else if (curr == QStringLiteral("CA$")) rawCurrency = QStringLiteral("CAD");
        else if (curr == QStringLiteral("£")) rawCurrency = QStringLiteral("GBP");
        else if (curr == QStringLiteral("€")) rawCurrency = QStringLiteral("EUR");
        else if (curr == QStringLiteral("₹")) rawCurrency = QStringLiteral("INR");
        else if (!curr.isEmpty()) rawCurrency = curr;
    } else {
        auto singleMatch = singleRegex.match(salaryText);
        if (!singleMatch.hasMatch()) return std::nullopt;

        const QString curr = singleMatch.captured(1).trimmed();
        const QString amountStr = singleMatch.captured(2).remove(QLatin1Char(','));
        const QString intervalStr = singleMatch.captured(3).trimmed();

        minAmount = amountStr.toDouble();
        maxAmount = minAmount;

        if (intervalStr == QLatin1String("/hr")) interval = QStringLiteral("hourly");
        else if (intervalStr == QLatin1String("/mo")) interval = QStringLiteral("monthly");
        else if (intervalStr == QLatin1String("/yr")) interval = QStringLiteral("yearly");

        if (curr == QStringLiteral("$")) rawCurrency = QStringLiteral("USD");
        else if (curr == QStringLiteral("CA$")) rawCurrency = QStringLiteral("CAD");
        else if (curr == QStringLiteral("£")) rawCurrency = QStringLiteral("GBP");
        else if (curr == QStringLiteral("€")) rawCurrency = QStringLiteral("EUR");
        else if (curr == QStringLiteral("₹")) rawCurrency = QStringLiteral("INR");
        else if (!curr.isEmpty()) rawCurrency = curr;
    }

    QJsonObject obj;
    obj[QStringLiteral("interval")] = interval;
    obj[QStringLiteral("min_amount")] = minAmount;
    obj[QStringLiteral("max_amount")] = maxAmount;
    obj[QStringLiteral("currency")] = rawCurrency;
    return obj;
}

std::optional<QJsonObject> JobParsingUtilities::parseIndeedCompensation(const QJsonObject &compObj) {
    if (compObj.isEmpty()) return std::nullopt;

    QJsonObject target;
    QString currency;

    if (compObj.contains(QLatin1String("baseSalary")) && compObj.value(QLatin1String("baseSalary")).isObject()) {
        target = compObj.value(QLatin1String("baseSalary")).toObject();
        currency = compObj.value(QLatin1String("currencyCode")).toString(QStringLiteral("USD"));
    } else if (compObj.contains(QLatin1String("estimated")) && compObj.value(QLatin1String("estimated")).isObject()) {
        const QJsonObject est = compObj.value(QLatin1String("estimated")).toObject();
        if (est.contains(QLatin1String("baseSalary")) && est.value(QLatin1String("baseSalary")).isObject()) {
            target = est.value(QLatin1String("baseSalary")).toObject();
            currency = est.value(QLatin1String("currencyCode")).toString(QStringLiteral("USD"));
        }
    }

    if (target.isEmpty()) return std::nullopt;

    const QString unit = target.value(QLatin1String("unitOfWork")).toString().toUpper();
    QString interval;
    if (unit == QLatin1String("YEAR")) interval = QStringLiteral("yearly");
    else if (unit == QLatin1String("HOUR")) interval = QStringLiteral("hourly");
    else if (unit == QLatin1String("MONTH")) interval = QStringLiteral("monthly");
    else if (unit == QLatin1String("WEEK")) interval = QStringLiteral("weekly");
    else if (unit == QLatin1String("DAY")) interval = QStringLiteral("daily");
    else interval = unit.toLower();

    double minVal = 0.0;
    double maxVal = 0.0;
    bool hasMin = false;
    bool hasMax = false;

    if (target.contains(QLatin1String("range")) && target.value(QLatin1String("range")).isObject()) {
        const QJsonObject rangeObj = target.value(QLatin1String("range")).toObject();
        if (rangeObj.contains(QLatin1String("min")) && !rangeObj.value(QLatin1String("min")).isNull()) {
            minVal = std::round(rangeObj.value(QLatin1String("min")).toDouble() * 100.0) / 100.0;
            hasMin = true;
        }
        if (rangeObj.contains(QLatin1String("max")) && !rangeObj.value(QLatin1String("max")).isNull()) {
            maxVal = std::round(rangeObj.value(QLatin1String("max")).toDouble() * 100.0) / 100.0;
            hasMax = true;
        }
    }

    QJsonObject res;
    res[QStringLiteral("interval")] = interval;
    res[QStringLiteral("min_amount")] = hasMin ? QJsonValue(minVal) : QJsonValue(QJsonValue::Null);
    res[QStringLiteral("max_amount")] = hasMax ? QJsonValue(maxVal) : QJsonValue(QJsonValue::Null);
    res[QStringLiteral("currency")] = currency.isEmpty() ? QStringLiteral("USD") : currency;
    return res;
}

QString JobParsingUtilities::mapJobTypeString(const QString &str) {
    const QString cleaned = str.toLower().remove(QLatin1Char('-')).remove(QLatin1Char(' '));
    if (cleaned.contains(QLatin1String("fulltime"))) return QStringLiteral("fulltime");
    if (cleaned.contains(QLatin1String("parttime"))) return QStringLiteral("parttime");
    if (cleaned.contains(QLatin1String("contract"))) return QStringLiteral("contract");
    if (cleaned.contains(QLatin1String("internship")) || cleaned.contains(QLatin1String("practica")) || cleaned.contains(QLatin1String("praktikum"))) return QStringLiteral("internship");
    if (cleaned.contains(QLatin1String("temporary"))) return QStringLiteral("temporary");
    if (cleaned.contains(QLatin1String("perdiem"))) return QStringLiteral("perdiem");
    if (cleaned.contains(QLatin1String("volunteer"))) return QStringLiteral("volunteer");
    return cleaned;
}

QStringList JobParsingUtilities::mapIndeedJobTypeAttributes(const QJsonArray &attributes) {
    QStringList result;
    for (const auto &val : attributes) {
        if (!val.isObject()) continue;
        const QJsonObject obj = val.toObject();
        const QString key = obj.value(QLatin1String("key")).toString();

        if (key == QLatin1String("CF3CP")) result.append(QStringLiteral("fulltime"));
        else if (key == QLatin1String("75GKK")) result.append(QStringLiteral("parttime"));
        else if (key == QLatin1String("NJXCK")) result.append(QStringLiteral("contract"));
        else if (key == QLatin1String("VDTG7")) result.append(QStringLiteral("internship"));
        else if (key == QLatin1String("4HKF7")) result.append(QStringLiteral("temporary"));
        else if (key == QLatin1String("TQKYQ")) result.append(QStringLiteral("perdiem"));
        else if (key == QLatin1String("UXQZ8")) result.append(QStringLiteral("volunteer"));
        else {
            const QString label = obj.value(QLatin1String("label")).toString();
            const QString mapped = mapJobTypeString(label);
            if (!mapped.isEmpty() && !result.contains(mapped)) {
                // If it matched one of our known types
                static const QStringList known = {
                    QStringLiteral("fulltime"), QStringLiteral("parttime"),
                    QStringLiteral("contract"), QStringLiteral("internship"),
                    QStringLiteral("temporary"), QStringLiteral("perdiem"),
                    QStringLiteral("volunteer")
                };
                if (known.contains(mapped)) {
                    result.append(mapped);
                }
            }
        }
    }
    return result;
}

} // namespace jobstarr
