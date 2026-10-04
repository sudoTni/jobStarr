#pragma once

#include <QString>

namespace jobstarr {

class PromptRenderer {
public:
    static bool validateTemplate(const QString &templateString, QString *errorMessage = nullptr);
    static QString render(const QString &templateString,
                          const QString &serializedJob,
                          const QString &resumeText,
                          QString *errorMessage = nullptr);

    static bool validateMakeMaterialsTemplate(const QString &templateString, QString *errorMessage = nullptr);
    static QString renderMakeMaterialsPrompt(const QString &templateString,
                                            const QString &professionalTitle,
                                            const QString &professionalSummary,
                                            const QString &keySkillsNormalized,
                                            const QString &serializedJob,
                                            const QString &resumeText,
                                            const QString &testimonialsText,
                                            QString *errorMessage = nullptr);

    static constexpr const char *kPlaceholderTargJD = "{targJD}";
    static constexpr const char *kPlaceholderMyResume = "{myResume}";
    static constexpr const char *kPlaceholderProfTitle = "{myProfessionalTitle}";
    static constexpr const char *kPlaceholderProfSummary = "{myProfessionalSummary}";
    static constexpr const char *kPlaceholderKeySkills = "{myKeySkills}";
    static constexpr const char *kPlaceholderTestimonials = "{myTestimonials}";
};

} // namespace jobstarr
