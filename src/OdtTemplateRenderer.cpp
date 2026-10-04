#include "OdtTemplateRenderer.h"
#include <zip.h>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <cstdlib>
#include <cstring>

namespace jobstarr {

QString OdtTemplateRenderer::xmlEscape(const QString &input) {
    QString out;
    out.reserve(input.size() + 16);
    for (const QChar &ch : input) {
        switch (ch.unicode()) {
            case '&': out.append(QStringLiteral("&amp;")); break;
            case '<': out.append(QStringLiteral("&lt;")); break;
            case '>': out.append(QStringLiteral("&gt;")); break;
            case '"': out.append(QStringLiteral("&quot;")); break;
            case '\'': out.append(QStringLiteral("&apos;")); break;
            default: out.append(ch); break;
        }
    }
    return out;
}

bool OdtTemplateRenderer::isArchiveValid(const QString &odtPath, QString *errorMessage) {
    if (!QFile::exists(odtPath)) {
        if (errorMessage) *errorMessage = QStringLiteral("ODT file does not exist: %1").arg(odtPath);
        return false;
    }
    int err = 0;
    zip_t *za = zip_open(odtPath.toLocal8Bit().constData(), ZIP_RDONLY, &err);
    if (!za) {
        if (errorMessage) *errorMessage = QStringLiteral("Failed to open ODT archive '%1' (zip error %2)").arg(odtPath).arg(err);
        return false;
    }
    zip_int64_t idx = zip_name_locate(za, "content.xml", 0);
    if (idx < 0) {
        if (errorMessage) *errorMessage = QStringLiteral("ODT archive '%1' is missing content.xml").arg(odtPath);
        zip_close(za);
        return false;
    }
    zip_close(za);
    return true;
}

QString OdtTemplateRenderer::extractContentXml(const QString &odtPath, QString *errorMessage) {
    if (!QFile::exists(odtPath)) {
        if (errorMessage) *errorMessage = QStringLiteral("ODT file does not exist: %1").arg(odtPath);
        return QString();
    }
    int err = 0;
    zip_t *za = zip_open(odtPath.toLocal8Bit().constData(), ZIP_RDONLY, &err);
    if (!za) {
        if (errorMessage) *errorMessage = QStringLiteral("Failed to open ODT '%1' (zip error %2)").arg(odtPath).arg(err);
        return QString();
    }
    zip_int64_t idx = zip_name_locate(za, "content.xml", 0);
    if (idx < 0) {
        if (errorMessage) *errorMessage = QStringLiteral("content.xml not found in '%1'").arg(odtPath);
        zip_close(za);
        return QString();
    }

    struct zip_stat st;
    zip_stat_init(&st);
    if (zip_stat_index(za, idx, 0, &st) < 0) {
        if (errorMessage) *errorMessage = QStringLiteral("Failed to stat content.xml in '%1'").arg(odtPath);
        zip_close(za);
        return QString();
    }

    zip_file_t *zf = zip_fopen_index(za, idx, 0);
    if (!zf) {
        if (errorMessage) *errorMessage = QStringLiteral("Failed to open content.xml in '%1'").arg(odtPath);
        zip_close(za);
        return QString();
    }

    QByteArray bytes;
    bytes.resize(st.size);
    zip_int64_t readBytes = zip_fread(zf, bytes.data(), st.size);
    zip_fclose(zf);
    zip_close(za);

    if (readBytes < 0 || static_cast<zip_uint64_t>(readBytes) != st.size) {
        if (errorMessage) *errorMessage = QStringLiteral("Failed to read all bytes of content.xml in '%1'").arg(odtPath);
        return QString();
    }

    return QString::fromUtf8(bytes);
}

bool OdtTemplateRenderer::validateResumeTemplate(const QString &templatePath, QString *errorMessage) {
    QString err;
    QString content = extractContentXml(templatePath, &err);
    if (content.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Invalid resume template: %1").arg(err);
        return false;
    }

    const QStringList required = {
        QLatin1String(kPlaceholderTitle),
        QLatin1String(kPlaceholderSummary),
        QLatin1String(kPlaceholderSkills)
    };

    for (const QString &ph : required) {
        if (!content.contains(ph)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Resume template is missing required placeholder: '%1'").arg(ph);
            }
            return false;
        }
    }
    return true;
}

bool OdtTemplateRenderer::validateCoverTemplate(const QString &templatePath, QString *errorMessage) {
    QString err;
    QString content = extractContentXml(templatePath, &err);
    if (content.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Invalid cover letter template: %1").arg(err);
        return false;
    }

    const QStringList required = {
        QLatin1String(kPlaceholderTitle),
        QLatin1String(kPlaceholderDate),
        QLatin1String(kPlaceholderAddress),
        QLatin1String(kPlaceholderCompany),
        QLatin1String(kPlaceholderCoverBody)
    };

    for (const QString &ph : required) {
        if (!content.contains(ph)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Cover letter template is missing required placeholder: '%1'").arg(ph);
            }
            return false;
        }
    }
    return true;
}

bool OdtTemplateRenderer::renderResume(const QString &templatePath,
                                      const QString &outputPath,
                                      const QString &professionalTitle,
                                      const QString &professionalSummary,
                                      const QString &skills,
                                      QString *errorMessage) {
    if (!validateResumeTemplate(templatePath, errorMessage)) {
        return false;
    }

    // Ensure output parent directory exists
    QFileInfo outInfo(outputPath);
    QDir().mkpath(outInfo.absolutePath());

    if (QFile::exists(outputPath)) {
        QFile::remove(outputPath);
    }

    if (!QFile::copy(templatePath, outputPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to copy resume template to '%1'").arg(outputPath);
        }
        return false;
    }

    QString content = extractContentXml(outputPath, errorMessage);
    if (content.isEmpty()) {
        QFile::remove(outputPath);
        return false;
    }

    // XML-escaped replacements
    content.replace(QLatin1String(kPlaceholderTitle), xmlEscape(professionalTitle));

    QString escapedSummary = xmlEscape(professionalSummary);
    escapedSummary.replace(QLatin1Char('\n'), QStringLiteral("<text:line-break/>"));
    content.replace(QLatin1String(kPlaceholderSummary), escapedSummary);

    content.replace(QLatin1String(kPlaceholderSkills), xmlEscape(skills));

    // Verify all placeholders were replaced
    if (content.contains(QLatin1String(kPlaceholderTitle)) ||
        content.contains(QLatin1String(kPlaceholderSummary)) ||
        content.contains(QLatin1String(kPlaceholderSkills))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unresolved resume placeholders remain after rendering.");
        }
        QFile::remove(outputPath);
        return false;
    }

    // Write updated content.xml into zip
    int err = 0;
    zip_t *za = zip_open(outputPath.toLocal8Bit().constData(), 0, &err);
    if (!za) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open '%1' for modification (zip error %2)").arg(outputPath).arg(err);
        }
        QFile::remove(outputPath);
        return false;
    }

    zip_int64_t idx = zip_name_locate(za, "content.xml", 0);
    const QByteArray utf8Bytes = content.toUtf8();
    char *buf = static_cast<char *>(std::malloc(utf8Bytes.size()));
    std::memcpy(buf, utf8Bytes.constData(), utf8Bytes.size());

    zip_source_t *zs = zip_source_buffer(za, buf, utf8Bytes.size(), 1); // 1 = libzip frees buf
    if (!zs || zip_file_replace(za, idx, zs, 0) < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to replace content.xml in '%1': %2")
                                .arg(outputPath, QString::fromUtf8(zip_strerror(za)));
        }
        if (zs) zip_source_free(zs); else std::free(buf);
        zip_close(za);
        QFile::remove(outputPath);
        return false;
    }

    if (zip_close(za) < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to finalize zip file '%1'").arg(outputPath);
        }
        QFile::remove(outputPath);
        return false;
    }

    return isArchiveValid(outputPath, errorMessage);
}

bool OdtTemplateRenderer::renderCoverLetter(const QString &templatePath,
                                           const QString &outputPath,
                                           const QString &professionalTitle,
                                           const QString &todaysDate,
                                           const QString &companyAddress,
                                           const QString &companyName,
                                           const QString &coverBody,
                                           QString *errorMessage) {
    if (!validateCoverTemplate(templatePath, errorMessage)) {
        return false;
    }

    // Ensure output parent directory exists
    QFileInfo outInfo(outputPath);
    QDir().mkpath(outInfo.absolutePath());

    if (QFile::exists(outputPath)) {
        QFile::remove(outputPath);
    }

    if (!QFile::copy(templatePath, outputPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to copy cover letter template to '%1'").arg(outputPath);
        }
        return false;
    }

    QString content = extractContentXml(outputPath, errorMessage);
    if (content.isEmpty()) {
        QFile::remove(outputPath);
        return false;
    }

    // XML-escaped replacements
    content.replace(QLatin1String(kPlaceholderTitle), xmlEscape(professionalTitle));
    content.replace(QLatin1String(kPlaceholderDate), xmlEscape(todaysDate));
    content.replace(QLatin1String(kPlaceholderAddress), xmlEscape(companyAddress));
    content.replace(QLatin1String(kPlaceholderCompany), xmlEscape(companyName));

    // Multi-paragraph cover letter replacement
    QStringList paras;
    const QStringList rawLines = coverBody.split(QLatin1Char('\n'));
    QString curPara;
    for (const QString &line : rawLines) {
        if (line.trimmed().isEmpty()) {
            if (!curPara.trimmed().isEmpty()) {
                paras.append(curPara.trimmed());
                curPara.clear();
            }
        } else {
            if (!curPara.isEmpty()) {
                curPara += QLatin1Char('\n');
            }
            curPara += line;
        }
    }
    if (!curPara.trimmed().isEmpty()) {
        paras.append(curPara.trimmed());
    }

    int idx = content.indexOf(QLatin1String(kPlaceholderCoverBody));
    if (idx != -1) {
        int pStart = content.lastIndexOf(QStringLiteral("<text:p"), idx);
        int pEnd = content.indexOf(QStringLiteral("</text:p>"), idx);
        if (pStart != -1 && pEnd != -1 && pStart < idx && pEnd > idx) {
            int pEndClose = pEnd + 9; // length of "</text:p>"
            QString pPrefix = content.mid(pStart, idx - pStart);
            QString pSuffix = content.mid(idx + qstrlen(kPlaceholderCoverBody), pEndClose - (idx + qstrlen(kPlaceholderCoverBody)));
            int tagClose = content.indexOf(QLatin1Char('>'), pStart);
            QString emptySeparator = (tagClose != -1 && tagClose < idx)
                                        ? content.mid(pStart, tagClose - pStart) + QStringLiteral("/>")
                                        : QStringLiteral("<text:p/>");

            QString replacement;
            for (int i = 0; i < paras.size(); ++i) {
                if (i > 0) {
                    replacement += emptySeparator;
                }
                QString escapedPara = xmlEscape(paras[i]);
                escapedPara.replace(QLatin1Char('\n'), QStringLiteral("<text:line-break/>"));
                replacement += pPrefix + escapedPara + pSuffix;
            }
            content.replace(pStart, pEndClose - pStart, replacement);
        } else {
            QString replacement;
            for (int i = 0; i < paras.size(); ++i) {
                if (i > 0) replacement += QStringLiteral("<text:p/>");
                QString escaped = xmlEscape(paras[i]);
                escaped.replace(QLatin1Char('\n'), QStringLiteral("<text:line-break/>"));
                replacement += QStringLiteral("<text:p>") + escaped + QStringLiteral("</text:p>");
            }
            content.replace(QLatin1String(kPlaceholderCoverBody), replacement);
        }
    }

    // Verify all required placeholders were replaced
    if (content.contains(QLatin1String(kPlaceholderTitle)) ||
        content.contains(QLatin1String(kPlaceholderDate)) ||
        content.contains(QLatin1String(kPlaceholderAddress)) ||
        content.contains(QLatin1String(kPlaceholderCompany)) ||
        content.contains(QLatin1String(kPlaceholderCoverBody))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Unresolved cover letter placeholders remain after rendering.");
        }
        QFile::remove(outputPath);
        return false;
    }

    // Write updated content.xml into zip
    int err = 0;
    zip_t *za = zip_open(outputPath.toLocal8Bit().constData(), 0, &err);
    if (!za) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open '%1' for modification (zip error %2)").arg(outputPath).arg(err);
        }
        QFile::remove(outputPath);
        return false;
    }

    zip_int64_t zipIdx = zip_name_locate(za, "content.xml", 0);
    const QByteArray utf8Bytes = content.toUtf8();
    char *buf = static_cast<char *>(std::malloc(utf8Bytes.size()));
    std::memcpy(buf, utf8Bytes.constData(), utf8Bytes.size());

    zip_source_t *zs = zip_source_buffer(za, buf, utf8Bytes.size(), 1);
    if (!zs || zip_file_replace(za, zipIdx, zs, 0) < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to replace content.xml in '%1': %2")
                                .arg(outputPath, QString::fromUtf8(zip_strerror(za)));
        }
        if (zs) zip_source_free(zs); else std::free(buf);
        zip_close(za);
        QFile::remove(outputPath);
        return false;
    }

    if (zip_close(za) < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to finalize zip file '%1'").arg(outputPath);
        }
        QFile::remove(outputPath);
        return false;
    }

    return isArchiveValid(outputPath, errorMessage);
}

} // namespace jobstarr
