#pragma once

#include <QString>
#include <QStringList>

namespace jobstarr {

class OdtTemplateRenderer {
public:
    static QString xmlEscape(const QString &input);

    static bool validateResumeTemplate(const QString &templatePath, QString *errorMessage = nullptr);
    static bool validateCoverTemplate(const QString &templatePath, QString *errorMessage = nullptr);

    static bool renderResume(const QString &templatePath,
                             const QString &outputPath,
                             const QString &professionalTitle,
                             const QString &professionalSummary,
                             const QString &skills,
                             QString *errorMessage = nullptr);

    static bool renderCoverLetter(const QString &templatePath,
                                  const QString &outputPath,
                                  const QString &professionalTitle,
                                  const QString &todaysDate,
                                  const QString &companyAddress,
                                  const QString &companyName,
                                  const QString &coverBody,
                                  QString *errorMessage = nullptr);

    static bool isArchiveValid(const QString &odtPath, QString *errorMessage = nullptr);
    static QString extractContentXml(const QString &odtPath, QString *errorMessage = nullptr);

    // Placeholders
    static constexpr const char *kPlaceholderTitle = "{{custom_prof_title}}";
    static constexpr const char *kPlaceholderSummary = "{{custom_prof_summary}}";
    static constexpr const char *kPlaceholderSkills = "{{custom_skills}}";
    static constexpr const char *kPlaceholderDate = "{{todays_date}}";
    static constexpr const char *kPlaceholderAddress = "{{job_company_address}}";
    static constexpr const char *kPlaceholderCompany = "{{job_company}}";
    static constexpr const char *kPlaceholderCoverBody = "{{cover_body}}";
};

} // namespace jobstarr
