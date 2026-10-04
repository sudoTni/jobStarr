#pragma once

#include <QString>
#include <optional>

namespace jobstarr {

struct JobJudgeResult {
    QString jobTitle;
    bool isVeryHighlyAligned{false};
    QString rationale;
    double confidence{0.0};

    static std::optional<JobJudgeResult> parseLlmResponse(const QString &rawResponse, QString *errorMessage = nullptr);
};

} // namespace jobstarr
