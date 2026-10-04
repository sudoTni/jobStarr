#pragma once

#include <QString>

namespace jobstarr {

class ProjectPaths {
public:
    static QString projectRoot();
    static void setCustomProjectRoot(const QString &path);

    static QString candidateDataDir();
    static QString promptsDir();
    static QString templatesDir();
    static QString outputDir();

    static QString defaultResumeTemplatePath();
    static QString defaultCoverLetterTemplatePath();

private:
    static QString s_customProjectRoot;
};

} // namespace jobstarr
