#include "CompanyAddressLookup.h"
#include <QRegularExpression>

namespace jobstarr {

QString CompanyAddressLookup::buildUserPrompt(const JobRecord &job) {
    QString prompt = QStringLiteral(
        "Find the current professional mailing/street address for the employer\n"
        "associated with this job posting.\n\n");

    if (!job.companyName().isEmpty()) {
        prompt += QStringLiteral("Company: %1\n").arg(job.companyName());
    }
    if (!job.title().isEmpty()) {
        prompt += QStringLiteral("Job title: %1\n").arg(job.title());
    }
    if (!job.location().isEmpty()) {
        prompt += QStringLiteral("Job location: %1\n").arg(job.location());
    }
    if (!job.companyUrl().isEmpty()) {
        prompt += QStringLiteral("Company URL: %1\n").arg(job.companyUrl());
    }
    if (!job.companyUrlDirect().isEmpty()) {
        prompt += QStringLiteral("Corporate URL: %1\n").arg(job.companyUrlDirect());
    }
    if (!job.jobUrl().isEmpty()) {
        prompt += QStringLiteral("Job URL: %1\n").arg(job.jobUrl());
    }

    prompt += QStringLiteral(
        "\nUse your search capability to identify the correct employer.\n\n"
        "Return ONLY the final address as one single line suitable for placement\n"
        "at the top of a cover letter.\n\n"
        "Do not include commentary, labels, citations, Markdown, or explanation.\n\n"
        "If a reliable address cannot be established, return:\n"
        "UNKNOWN\n");

    return prompt;
}

std::optional<QString> CompanyAddressLookup::parseResponse(const QString &rawResponse, QString *errorMessage) {
    QString cleaned = rawResponse.trimmed();

    if (cleaned.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Company address lookup returned empty response.");
        return std::nullopt;
    }

    // Strip code fences ``` if present
    if (cleaned.startsWith(QStringLiteral("```"))) {
        int firstNl = cleaned.indexOf(QLatin1Char('\n'));
        if (firstNl != -1) {
            cleaned = cleaned.mid(firstNl + 1);
        }
        if (cleaned.endsWith(QStringLiteral("```"))) {
            cleaned.chop(3);
        }
        cleaned = cleaned.trimmed();
    }

    // Strip common address prefix labels (e.g., "Address: ", "Company address: ")
    static const QRegularExpression labelRegex(
        QStringLiteral("^(address|company\\s+address|employer\\s+address|mailing\\s+address)\\s*:\\s*"),
        QRegularExpression::CaseInsensitiveOption
    );
    cleaned.remove(labelRegex);
    cleaned = cleaned.trimmed();

    // Strip Markdown bullet prefixes
    static const QRegularExpression bulletRegex(QStringLiteral("^[-*+•]\\s*"));
    cleaned.remove(bulletRegex);
    cleaned = cleaned.trimmed();

    // Strip surrounding quotes
    if ((cleaned.startsWith(QLatin1Char('"')) && cleaned.endsWith(QLatin1Char('"'))) ||
        (cleaned.startsWith(QLatin1Char('\'')) && cleaned.endsWith(QLatin1Char('\'')))) {
        cleaned = cleaned.mid(1, cleaned.length() - 2).trimmed();
    }

    // Normalize CR, LF, tabs, and multiple spaces into a single space
    static const QRegularExpression whitespaceRegex(QStringLiteral("[\\r\\n\\t\\s]+"));
    cleaned = cleaned.replace(whitespaceRegex, QStringLiteral(" ")).trimmed();

    if (cleaned.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Normalized address is empty.");
        return std::nullopt;
    }

    if (cleaned.compare(QStringLiteral("UNKNOWN"), Qt::CaseInsensitive) == 0) {
        if (errorMessage) *errorMessage = QStringLiteral("Search LLM returned UNKNOWN for company address.");
        return std::nullopt;
    }

    return cleaned;
}

} // namespace jobstarr
