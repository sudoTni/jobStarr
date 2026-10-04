#pragma once

#include <QString>
#include <QStringList>

namespace jobstarr {

class OutputManager {
public:
    explicit OutputManager(const QString &outputDir = QString());

    QString outputDirectory() const;
    void setOutputDirectory(const QString &dir);

    QString createStagingDirectory(QString *errorMessage = nullptr);
    bool cleanStagingDirectory(const QString &stagingDir);

    bool validateStagedArtifacts(const QString &stagingDir,
                                const QString &resumeBasename,
                                const QString &coverBasename,
                                QString *errorMessage = nullptr);

    bool publishStagedArtifacts(const QString &stagingDir,
                               const QString &resumeBasename,
                               const QString &coverBasename,
                               QStringList *publishedFiles = nullptr,
                               QString *errorMessage = nullptr);

    bool openOutputFolder(QString *errorMessage = nullptr);

private:
    QString m_outputDir;
};

} // namespace jobstarr
