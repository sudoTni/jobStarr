#include "OutputManager.h"
#include "ProjectPaths.h"
#include "OdtTemplateRenderer.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <QUrl>
#include <QDesktopServices>

namespace jobstarr {

OutputManager::OutputManager(const QString &outputDir)
    : m_outputDir(outputDir) {}

QString OutputManager::outputDirectory() const {
    if (!m_outputDir.isEmpty()) {
        return m_outputDir;
    }
    return ProjectPaths::outputDir();
}

void OutputManager::setOutputDirectory(const QString &dir) {
    m_outputDir = dir;
}

QString OutputManager::createStagingDirectory(QString *errorMessage) {
    const QString baseOut = outputDirectory();
    QDir().mkpath(baseOut);

    const QString uniqueId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString stagingPath = QDir(baseOut).filePath(QStringLiteral(".jobstarr-tmp-%1").arg(uniqueId));

    if (!QDir().mkpath(stagingPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to create temporary staging directory: %1").arg(stagingPath);
        }
        return QString();
    }
    return stagingPath;
}

bool OutputManager::cleanStagingDirectory(const QString &stagingDir) {
    if (stagingDir.isEmpty() || !QDir(stagingDir).exists()) {
        return true;
    }
    QDir dir(stagingDir);
    return dir.removeRecursively();
}

bool OutputManager::validateStagedArtifacts(const QString &stagingDir,
                                           const QString &resumeBasename,
                                           const QString &coverBasename,
                                           QString *errorMessage) {
    if (stagingDir.isEmpty() || !QDir(stagingDir).exists()) {
        if (errorMessage) *errorMessage = QStringLiteral("Staging directory does not exist: %1").arg(stagingDir);
        return false;
    }

    const QString resumeOdt = QDir(stagingDir).filePath(resumeBasename + QStringLiteral(".odt"));
    const QString resumePdf = QDir(stagingDir).filePath(resumeBasename + QStringLiteral(".pdf"));
    const QString coverOdt = QDir(stagingDir).filePath(coverBasename + QStringLiteral(".odt"));
    const QString coverPdf = QDir(stagingDir).filePath(coverBasename + QStringLiteral(".pdf"));

    const QStringList files = {resumeOdt, resumePdf, coverOdt, coverPdf};
    for (const QString &file : files) {
        if (!QFile::exists(file)) {
            if (errorMessage) *errorMessage = QStringLiteral("Staged artifact missing: %1").arg(file);
            return false;
        }
        QFileInfo fi(file);
        if (fi.size() <= 0) {
            if (errorMessage) *errorMessage = QStringLiteral("Staged artifact is empty (0 bytes): %1").arg(file);
            return false;
        }
    }

    // Verify ODT archives are valid ZIP packages
    QString odtErr;
    if (!OdtTemplateRenderer::isArchiveValid(resumeOdt, &odtErr)) {
        if (errorMessage) *errorMessage = QStringLiteral("Staged resume ODT is corrupt: %1").arg(odtErr);
        return false;
    }
    if (!OdtTemplateRenderer::isArchiveValid(coverOdt, &odtErr)) {
        if (errorMessage) *errorMessage = QStringLiteral("Staged cover letter ODT is corrupt: %1").arg(odtErr);
        return false;
    }

    return true;
}

bool OutputManager::publishStagedArtifacts(const QString &stagingDir,
                                          const QString &resumeBasename,
                                          const QString &coverBasename,
                                          QStringList *publishedFiles,
                                          QString *errorMessage) {
    if (!validateStagedArtifacts(stagingDir, resumeBasename, coverBasename, errorMessage)) {
        return false;
    }

    const QString destDir = outputDirectory();
    QDir().mkpath(destDir);

    const QStringList filenames = {
        resumeBasename + QStringLiteral(".odt"),
        resumeBasename + QStringLiteral(".pdf"),
        coverBasename + QStringLiteral(".odt"),
        coverBasename + QStringLiteral(".pdf")
    };

    QStringList finalized;

    for (const QString &filename : filenames) {
        const QString stagedFile = QDir(stagingDir).filePath(filename);
        const QString finalFile = QDir(destDir).filePath(filename);

        if (QFile::exists(finalFile)) {
            if (!QFile::remove(finalFile)) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Failed to replace existing output file: %1").arg(finalFile);
                }
                return false;
            }
        }

        if (!QFile::copy(stagedFile, finalFile)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Failed to move staged file '%1' to '%2'").arg(stagedFile, finalFile);
            }
            return false;
        }

        finalized.append(finalFile);
    }

    // Clean up staging directory after successful publish
    cleanStagingDirectory(stagingDir);

    if (publishedFiles) {
        *publishedFiles = finalized;
    }

    return true;
}

bool OutputManager::openOutputFolder(QString *errorMessage) {
    const QString path = outputDirectory();
    QDir().mkpath(path);

    const QUrl url = QUrl::fromLocalFile(path);
    if (!QDesktopServices::openUrl(url)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open output directory '%1' with desktop file manager.").arg(path);
        }
        return false;
    }
    return true;
}

} // namespace jobstarr
