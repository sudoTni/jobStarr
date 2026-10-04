#pragma once

#include <QString>
#include <optional>
#include "JobRecord.h"

namespace jobstarr {

class CompanyAddressLookup {
public:
    static QString buildUserPrompt(const JobRecord &job);
    static std::optional<QString> parseResponse(const QString &rawResponse, QString *errorMessage = nullptr);
};

} // namespace jobstarr
