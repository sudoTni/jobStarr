#include "PdfConverter.h"
#include <QStandardPaths>
#include <QProcess>
#include <QFileInfo>
#include <QFile>
#include <QDir>

namespace jobstarr {

bool DefaultPdfProcessRunner::run(const QString &program,
                                 const QStringList &arguments,
                                 int timeoutMs,
                                 int *exitCode,
                                 QString *stdErr) {
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted(timeoutMs)) {
        if (stdErr) *stdErr = QStringLiteral("Failed to start process: %1").arg(process.errorString());
        return false;
    }

    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        if (stdErr) *stdErr = QStringLiteral("Process timed out after %1 ms.").arg(timeoutMs);
        return false;
    }

    if (exitCode) {
        *exitCode = process.exitCode();
    }
    if (stdErr) {
        *stdErr = QString::fromUtf8(process.readAllStandardError());
    }
    return (process.exitStatus() == QProcess::NormalExit);
}

PdfConverter::PdfConverter(std::shared_ptr<IPdfProcessRunner> runner)
    : m_runner(runner ? runner : std::make_shared<DefaultPdfProcessRunner>()) {}

QString PdfConverter::findConverterExecutable() {
    QString exe = QStandardPaths::findExecutable(QStringLiteral("libreoffice"));
    if (exe.isEmpty()) {
        exe = QStandardPaths::findExecutable(QStringLiteral("soffice"));
    }
    return exe;
}

bool PdfConverter::convertToPdf(const QString &odtFilePath,
                                const QString &outputDir,
                                QString *resultingPdfPath,
                                QString *errorMessage,
                                int timeoutMs) {
    if (!QFile::exists(odtFilePath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Source ODT file does not exist: %1").arg(odtFilePath);
        }
        return false;
    }

    const QString converterExe = findConverterExecutable();
    if (converterExe.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PDF conversion requires LibreOffice. "
                                           "Install LibreOffice or make 'libreoffice'/'soffice' available in PATH.");
        }
        return false;
    }

    QDir().mkpath(outputDir);

    const QStringList arguments = {
        QStringLiteral("--headless"),
        QStringLiteral("--convert-to"),
        QStringLiteral("pdf"),
        QStringLiteral("--outdir"),
        outputDir,
        odtFilePath
    };

    int exitCode = -1;
    QString processError;
    const bool finishedNormal = m_runner->run(converterExe, arguments, timeoutMs, &exitCode, &processError);

    if (!finishedNormal || exitCode != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("LibreOffice exited with code %1: %2")
                                .arg(exitCode)
                                .arg(processError.trimmed().isEmpty() ? QStringLiteral("Unknown conversion error") : processError.trimmed());
        }
        return false;
    }

    // Determine expected PDF filename: <basename>.pdf
    QFileInfo odtInfo(odtFilePath);
    const QString expectedPdf = QDir(outputDir).filePath(odtInfo.completeBaseName() + QStringLiteral(".pdf"));

    if (!QFile::exists(expectedPdf)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Expected output PDF does not exist: %1").arg(expectedPdf);
        }
        return false;
    }

    QFileInfo pdfInfo(expectedPdf);
    if (pdfInfo.size() <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Generated PDF is empty (0 bytes): %1").arg(expectedPdf);
        }
        return false;
    }

    if (resultingPdfPath) {
        *resultingPdfPath = expectedPdf;
    }

    return true;
}

} // namespace jobstarr
