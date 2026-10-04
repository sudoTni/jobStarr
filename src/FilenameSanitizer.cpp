#include "FilenameSanitizer.h"
#include <QRegularExpression>
#include <QFileInfo>

namespace jobstarr {

bool FilenameSanitizer::isSafeBasename(const QString &name) {
    if (name.isEmpty() || name.length() > kMaxBasenameLength) {
        return false;
    }
    if (name == QLatin1String(".") || name == QLatin1String("..")) {
        return false;
    }
    if (name.startsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char('.'))) {
        return false;
    }
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\')) || name.contains(QStringLiteral(".."))) {
        return false;
    }

    // Only allow alphanumeric, dash, underscore, space, parenthesis, and safe internal dots
    static const QRegularExpression safeRegex(QStringLiteral("^[a-zA-Z0-9_ \\-\\.\\(\\)]+$"));
    return safeRegex.match(name).hasMatch();
}

std::optional<QString> FilenameSanitizer::sanitize(const QString &rawName, QString *errorMessage) {
    QString cleaned = rawName.trimmed();

    if (cleaned.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Generated filename is empty.");
        return std::nullopt;
    }

    // Strip surrounding quotes
    if ((cleaned.startsWith(QLatin1Char('"')) && cleaned.endsWith(QLatin1Char('"'))) ||
        (cleaned.startsWith(QLatin1Char('\'')) && cleaned.endsWith(QLatin1Char('\'')))) {
        cleaned = cleaned.mid(1, cleaned.length() - 2).trimmed();
    }

    // Strip trailing .odt and .pdf extensions (case-insensitive)
    while (cleaned.endsWith(QStringLiteral(".odt"), Qt::CaseInsensitive) ||
           cleaned.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
        cleaned.chop(4);
        cleaned = cleaned.trimmed();
    }

    // Remove control characters (ASCII < 32 or 127)
    QString filtered;
    filtered.reserve(cleaned.size());
    for (const QChar &ch : cleaned) {
        ushort unicode = ch.unicode();
        if (unicode >= 32 && unicode != 127) {
            filtered.append(ch);
        }
    }
    cleaned = filtered;

    // Replace path separators / and \ with underscore
    cleaned.replace(QLatin1Char('/'), QLatin1Char('_'));
    cleaned.replace(QLatin1Char('\\'), QLatin1Char('_'));

    // Replace consecutive dots with underscore
    static const QRegularExpression multiDots(QStringLiteral("\\.{2,}"));
    cleaned.replace(multiDots, QStringLiteral("_"));

    // Replace unsafe characters with underscore
    static const QRegularExpression unsafeChars(QStringLiteral("[^a-zA-Z0-9_ \\-\\.\\(\\)]"));
    cleaned.replace(unsafeChars, QStringLiteral("_"));

    // Collapse multiple underscores
    static const QRegularExpression multiUnderscores(QStringLiteral("_{2,}"));
    cleaned.replace(multiUnderscores, QStringLiteral("_"));

    // Strip leading and trailing dots, spaces, and underscores
    static const QRegularExpression trimBoundary(QStringLiteral("^[._ ]+|[._ ]+$"));
    cleaned.remove(trimBoundary);

    if (cleaned.isEmpty() || cleaned == QLatin1String(".") || cleaned == QLatin1String("..")) {
        if (errorMessage) *errorMessage = QStringLiteral("Sanitized filename is empty or invalid.");
        return std::nullopt;
    }

    // Truncate to kMaxBasenameLength (120 chars)
    if (cleaned.length() > kMaxBasenameLength) {
        cleaned.truncate(kMaxBasenameLength);
        cleaned.remove(trimBoundary);
    }

    if (cleaned.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Sanitized filename is empty after truncation.");
        return std::nullopt;
    }

    return cleaned;
}

} // namespace jobstarr
