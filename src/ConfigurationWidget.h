#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include "ConfigManager.h"

namespace jobstarr {

class ConfigurationWidget : public QWidget {
    Q_OBJECT
public:
    explicit ConfigurationWidget(QWidget *parent = nullptr);

    void setConfig(const AppConfig &config);
    AppConfig config() const;

signals:
    void configSaved(const AppConfig &config);

private slots:
    void onSaveClicked();
    void toggleApiKeyVisibility();
    void toggleSearchApiKeyVisibility();
    void onBrowseResumeTemplate();
    void onBrowseCoverTemplate();

private:
    void setupUi();

    // Primary LLM
    QLineEdit *m_endpointEdit{nullptr};
    QLineEdit *m_modelEdit{nullptr};
    QLineEdit *m_apiKeyEdit{nullptr};
    QPushButton *m_toggleApiKeyButton{nullptr};
    QLineEdit *m_reasoningEffortEdit{nullptr};
    QSpinBox *m_timeoutSpinBox{nullptr};

    // Search LLM
    QLineEdit *m_searchEndpointEdit{nullptr};
    QLineEdit *m_searchModelEdit{nullptr};
    QLineEdit *m_searchApiKeyEdit{nullptr};
    QPushButton *m_toggleSearchApiKeyButton{nullptr};
    QLineEdit *m_searchReasoningEffortEdit{nullptr};

    // Prompts
    QPlainTextEdit *m_systemPromptEdit{nullptr};
    QPlainTextEdit *m_jobJudgePromptEdit{nullptr};
    QPlainTextEdit *m_makeMaterialsPromptEdit{nullptr};

    // Candidate Profile
    QLineEdit *m_professionalTitleEdit{nullptr};
    QPlainTextEdit *m_professionalSummaryEdit{nullptr};
    QPlainTextEdit *m_keySkillsEdit{nullptr};
    QPlainTextEdit *m_myResumeEdit{nullptr};
    QPlainTextEdit *m_testimonialsEdit{nullptr};

    // Application Templates
    QLineEdit *m_resumeTemplateEdit{nullptr};
    QPushButton *m_browseResumeButton{nullptr};
    QLineEdit *m_coverTemplateEdit{nullptr};
    QPushButton *m_browseCoverButton{nullptr};

    QPushButton *m_saveButton{nullptr};
    QLabel *m_statusLabel{nullptr};

    QString m_version{"0.2.0"};
    ConfigManager m_configManager;
};

} // namespace jobstarr
