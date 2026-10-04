#include "ProjectPaths.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>

namespace jobstarr {

QString ProjectPaths::s_customProjectRoot;

void ProjectPaths::setCustomProjectRoot(const QString &path) {
    s_customProjectRoot = path;
}

QString ProjectPaths::projectRoot() {
    if (!s_customProjectRoot.isEmpty()) {
        return s_customProjectRoot;
    }

    // 1. Explicit JOBSTARR_PROJECT_ROOT environment variable
    const QString envRoot = qEnvironmentVariable("JOBSTARR_PROJECT_ROOT");
    if (!envRoot.isEmpty() && QDir(envRoot).exists()) {
        return QDir(envRoot).canonicalPath();
    }

    // 2. CMake-provided source/project root
#ifdef JOBSTARR_SOURCE_DIR
    const QString sourceDir = QStringLiteral(JOBSTARR_SOURCE_DIR);
    if (!sourceDir.isEmpty() && QDir(sourceDir).exists()) {
        return QDir(sourceDir).canonicalPath();
    }
#endif

    // 3. Repository-layout discovery relative to executable
    const QString appDir = QCoreApplication::applicationDirPath();
    QDir dir(appDir);
    // Check appDir itself, then walk up parents
    for (int i = 0; i < 4; ++i) {
        if (dir.exists(QStringLiteral("templates")) &&
            dir.exists(QStringLiteral("candidate_data"))) {
            return dir.canonicalPath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }

    return appDir;
}

QString ProjectPaths::candidateDataDir() {
    return QDir(projectRoot()).filePath(QStringLiteral("candidate_data"));
}

QString ProjectPaths::promptsDir() {
    return QDir(projectRoot()).filePath(QStringLiteral("prompts"));
}

QString ProjectPaths::templatesDir() {
    return QDir(projectRoot()).filePath(QStringLiteral("templates"));
}

QString ProjectPaths::outputDir() {
    const QString path = QDir(projectRoot()).filePath(QStringLiteral("output"));
    QDir().mkpath(path);
    return path;
}

QString ProjectPaths::defaultResumeTemplatePath() {
    return QDir(templatesDir()).filePath(QStringLiteral("Candidate_Resume000-TEMPLATE.odt"));
}

QString ProjectPaths::defaultCoverLetterTemplatePath() {
    return QDir(templatesDir()).filePath(QStringLiteral("Candidate_Cover000-TEMPLATE.odt"));
}

} // namespace jobstarr
