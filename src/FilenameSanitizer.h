#pragma once

#include <QString>
#include <optional>

namespace jobstarr {

class FilenameSanitizer {
public:
    static std::optional<QString> sanitize(const QString &rawName, QString *errorMessage = nullptr);
    static bool isSafeBasename(const QString &name);

    static constexpr int kMaxBasenameLength = 120;
};

} // namespace jobstarr
