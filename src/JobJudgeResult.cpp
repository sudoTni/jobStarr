#include "JobJudgeResult.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

namespace jobstarr {

static QString stripCodeFences(const QString &input) {
    QString trimmed = input.trimmed();
    // Matches ```json ... ``` or ``` ... ```
    static const QRegularExpression fenceRegex(
        QStringLiteral(R"(^```(?:json)?\s*\n?(.*?)\n?```$)"),
        QRegularExpression::DotMatchesEverythingOption
    );
    const auto match = fenceRegex.match(trimmed);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }
    return trimmed;
}

std::optional<JobJudgeResult> JobJudgeResult::parseLlmResponse(const QString &rawResponse, QString *errorMessage) {
    const QString cleaned = stripCodeFences(rawResponse);
    if (cleaned.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("LLM response is empty.");
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(cleaned.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid JSON in LLM response: %1").arg(parseError.errorString());
        }
        return std::nullopt;
    }

    if (!doc.isArray()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("LLM response must be a top-level JSON array.");
        }
        return std::nullopt;
    }

    const QJsonArray arr = doc.array();
    if (arr.size() != 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("LLM response array must contain exactly 1 object, found %1.").arg(arr.size());
        }
        return std::nullopt;
    }

    if (!arr.at(0).isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Array element must be a JSON object.");
        }
        return std::nullopt;
    }

    const QJsonObject obj = arr.at(0).toObject();

    // Required fields: jobTitle (string), isVeryHighlyAligned (bool), rationale (string), confidence (number 0..1)
    if (!obj.contains(QLatin1String("jobTitle"))) {
        if (errorMessage) *errorMessage = QStringLiteral("Missing required field 'jobTitle'.");
        return std::nullopt;
    }
    if (!obj.value(QLatin1String("jobTitle")).isString()) {
        if (errorMessage) *errorMessage = QStringLiteral("Field 'jobTitle' must be a string.");
        return std::nullopt;
    }

    if (!obj.contains(QLatin1String("isVeryHighlyAligned"))) {
        if (errorMessage) *errorMessage = QStringLiteral("Missing required field 'isVeryHighlyAligned'.");
        return std::nullopt;
    }
    if (!obj.value(QLatin1String("isVeryHighlyAligned")).isBool()) {
        if (errorMessage) *errorMessage = QStringLiteral("Field 'isVeryHighlyAligned' must be a boolean.");
        return std::nullopt;
    }

    if (!obj.contains(QLatin1String("rationale"))) {
        if (errorMessage) *errorMessage = QStringLiteral("Missing required field 'rationale'.");
        return std::nullopt;
    }
    if (!obj.value(QLatin1String("rationale")).isString()) {
        if (errorMessage) *errorMessage = QStringLiteral("Field 'rationale' must be a string.");
        return std::nullopt;
    }

    if (!obj.contains(QLatin1String("confidence"))) {
        if (errorMessage) *errorMessage = QStringLiteral("Missing required field 'confidence'.");
        return std::nullopt;
    }
    const QJsonValue confVal = obj.value(QLatin1String("confidence"));
    if (!confVal.isDouble()) {
        if (errorMessage) *errorMessage = QStringLiteral("Field 'confidence' must be a numeric value.");
        return std::nullopt;
    }

    const double conf = confVal.toDouble();
    if (conf < 0.0 || conf > 1.0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Field 'confidence' must be between 0.0 and 1.0 (found %1).").arg(conf);
        }
        return std::nullopt;
    }

    JobJudgeResult result;
    result.jobTitle = obj.value(QLatin1String("jobTitle")).toString();
    result.isVeryHighlyAligned = obj.value(QLatin1String("isVeryHighlyAligned")).toBool();
    result.rationale = obj.value(QLatin1String("rationale")).toString();
    result.confidence = conf;

    return result;
}

} // namespace jobstarr
