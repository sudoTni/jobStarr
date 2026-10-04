#include "PromptRenderer.h"
#include <QStringList>

namespace jobstarr {

bool PromptRenderer::validateTemplate(const QString &templateString, QString *errorMessage) {
    const QString targJD = QLatin1String(kPlaceholderTargJD);
    const QString myResume = QLatin1String(kPlaceholderMyResume);

    const bool hasTargJD = templateString.contains(targJD);
    const bool hasMyResume = templateString.contains(myResume);

    if (!hasTargJD && !hasMyResume) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Job Judge Prompt is missing required placeholders: %1 and %2.")
                                .arg(targJD, myResume);
        }
        return false;
    }
    if (!hasTargJD) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Job Judge Prompt is missing required placeholder: %1.")
                                .arg(targJD);
        }
        return false;
    }
    if (!hasMyResume) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Job Judge Prompt is missing required placeholder: %1.")
                                .arg(myResume);
        }
        return false;
    }

    return true;
}

QString PromptRenderer::render(const QString &templateString,
                              const QString &serializedJob,
                              const QString &resumeText,
                              QString *errorMessage) {
    if (!validateTemplate(templateString, errorMessage)) {
        return {};
    }

    // Literal string replacement without regex to preserve special characters
    QString rendered = templateString;
    rendered.replace(QLatin1String(kPlaceholderTargJD), serializedJob);
    rendered.replace(QLatin1String(kPlaceholderMyResume), resumeText);

    return rendered;
}

bool PromptRenderer::validateMakeMaterialsTemplate(const QString &templateString, QString *errorMessage) {
    const QStringList requiredPlaceholders = {
        QLatin1String(kPlaceholderProfTitle),
        QLatin1String(kPlaceholderProfSummary),
        QLatin1String(kPlaceholderKeySkills),
        QLatin1String(kPlaceholderTargJD),
        QLatin1String(kPlaceholderMyResume),
        QLatin1String(kPlaceholderTestimonials)
    };

    QStringList missing;
    for (const QString &ph : requiredPlaceholders) {
        if (!templateString.contains(ph)) {
            missing.append(ph);
        }
    }

    if (!missing.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Make Materials Prompt is missing required placeholders: %1.")
                                .arg(missing.join(QStringLiteral(", ")));
        }
        return false;
    }

    return true;
}

QString PromptRenderer::renderMakeMaterialsPrompt(const QString &templateString,
                                                 const QString &professionalTitle,
                                                 const QString &professionalSummary,
                                                 const QString &keySkillsNormalized,
                                                 const QString &serializedJob,
                                                 const QString &resumeText,
                                                 const QString &testimonialsText,
                                                 QString *errorMessage) {
    if (!validateMakeMaterialsTemplate(templateString, errorMessage)) {
        return {};
    }

    // Literal replacements on temporary copy without regex
    QString rendered = templateString;
    rendered.replace(QLatin1String(kPlaceholderProfTitle), professionalTitle);
    rendered.replace(QLatin1String(kPlaceholderProfSummary), professionalSummary);
    rendered.replace(QLatin1String(kPlaceholderKeySkills), keySkillsNormalized);
    rendered.replace(QLatin1String(kPlaceholderTargJD), serializedJob);
    rendered.replace(QLatin1String(kPlaceholderMyResume), resumeText);
    rendered.replace(QLatin1String(kPlaceholderTestimonials), testimonialsText);

    return rendered;
}

} // namespace jobstarr

