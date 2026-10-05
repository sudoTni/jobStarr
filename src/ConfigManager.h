#pragma once

#include <QString>

namespace jobstarr {

struct AppConfig {
    QString version{"0.2.1"};

    // Primary LLM
    QString apiEndpoint{"https://api.openai.com/v1/chat/completions"};
    QString model{"gpt-4o"};
    QString apiKey{""};
    QString reasoningEffort{""};
    int timeoutSeconds{300};

    // Search LLM
    QString searchEndpoint{""};
    QString searchModel{""};
    QString searchApiKey{""};
    QString searchReasoningEffort{""};
    int searchTimeoutSeconds{300};

    // Prompts
    QString systemPrompt{""};
    QString jobJudgePrompt{""};
    QString makeMaterialsPrompt{""};

    // Candidate Profile
    QString professionalTitle{""};
    QString professionalSummary{""};
    QString keySkills{""};
    QString myResume{""};
    QString testimonials{""};

    // Materials Templates
    QString resumeTemplatePath{""};
    QString coverLetterTemplatePath{""};
};

class ConfigManager {
public:
    explicit ConfigManager(const QString &customConfigPath = QString());

    QString configFilePath() const;
    void setCustomConfigPath(const QString &path);

    bool load(AppConfig &config, QString *errorMessage = nullptr);
    bool save(const AppConfig &config, QString *errorMessage = nullptr);

    static QString loadDefaultSystemPrompt();
    static QString loadDefaultJobJudgePrompt();
    static QString loadDefaultMakeMaterialsPrompt();
    static QString loadDefaultResume();
    static QString loadDefaultProfessionalTitle();
    static QString loadDefaultProfessionalSummary();
    static QString loadDefaultKeySkills();
    static QString loadDefaultTestimonials();

private:
    QString m_customConfigPath;
};

} // namespace jobstarr
