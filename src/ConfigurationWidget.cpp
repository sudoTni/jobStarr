#include "ConfigurationWidget.h"
#include "PromptRenderer.h"
#include "ProjectPaths.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QGroupBox>
#include <QFileDialog>
#include <QFileInfo>

namespace jobstarr {

ConfigurationWidget::ConfigurationWidget(QWidget *parent)
    : QWidget(parent) {
    setupUi();
}

void ConfigurationWidget::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *contentWidget = new QWidget(scrollArea);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(14);

    // 1. Primary LLM Group
    auto *primaryGroup = new QGroupBox(tr("Primary LLM (Job Judge & Make Materials)"), contentWidget);
    auto *primaryFormLayout = new QFormLayout(primaryGroup);
    primaryFormLayout->setLabelAlignment(Qt::AlignLeft);

    m_endpointEdit = new QLineEdit(primaryGroup);
    m_endpointEdit->setPlaceholderText(tr("https://api.openai.com/v1/chat/completions"));
    primaryFormLayout->addRow(tr("Endpoint:"), m_endpointEdit);

    m_modelEdit = new QLineEdit(primaryGroup);
    m_modelEdit->setPlaceholderText(tr("gpt-4o"));
    primaryFormLayout->addRow(tr("Model:"), m_modelEdit);

    auto *apiKeyLayout = new QHBoxLayout();
    m_apiKeyEdit = new QLineEdit(primaryGroup);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText(tr("sk-..."));
    apiKeyLayout->addWidget(m_apiKeyEdit);

    m_toggleApiKeyButton = new QPushButton(tr("Show"), primaryGroup);
    m_toggleApiKeyButton->setFixedWidth(60);
    connect(m_toggleApiKeyButton, &QPushButton::clicked, this, &ConfigurationWidget::toggleApiKeyVisibility);
    apiKeyLayout->addWidget(m_toggleApiKeyButton);

    primaryFormLayout->addRow(tr("API Key:"), apiKeyLayout);

    m_reasoningEffortEdit = new QLineEdit(primaryGroup);
    m_reasoningEffortEdit->setPlaceholderText(tr("e.g. low, medium, high (optional)"));
    primaryFormLayout->addRow(tr("Reasoning Effort:"), m_reasoningEffortEdit);

    m_timeoutSpinBox = new QSpinBox(primaryGroup);
    m_timeoutSpinBox->setRange(10, 1800);
    m_timeoutSpinBox->setValue(300);
    m_timeoutSpinBox->setSingleStep(15);
    m_timeoutSpinBox->setSuffix(tr(" seconds"));
    primaryFormLayout->addRow(tr("Timeout:"), m_timeoutSpinBox);

    contentLayout->addWidget(primaryGroup);

    // 2. Search LLM Group
    auto *searchGroup = new QGroupBox(tr("Search LLM (Company Address Lookup)"), contentWidget);
    auto *searchFormLayout = new QFormLayout(searchGroup);
    searchFormLayout->setLabelAlignment(Qt::AlignLeft);

    m_searchEndpointEdit = new QLineEdit(searchGroup);
    m_searchEndpointEdit->setPlaceholderText(tr("https://openrouter.ai/api/v1/chat/completions"));
    searchFormLayout->addRow(tr("Endpoint:"), m_searchEndpointEdit);

    m_searchModelEdit = new QLineEdit(searchGroup);
    m_searchModelEdit->setPlaceholderText(tr("perplexity/sonar or similar search model"));
    searchFormLayout->addRow(tr("Model:"), m_searchModelEdit);

    auto *searchApiKeyLayout = new QHBoxLayout();
    m_searchApiKeyEdit = new QLineEdit(searchGroup);
    m_searchApiKeyEdit->setEchoMode(QLineEdit::Password);
    m_searchApiKeyEdit->setPlaceholderText(tr("Search LLM API key"));
    searchApiKeyLayout->addWidget(m_searchApiKeyEdit);

    m_toggleSearchApiKeyButton = new QPushButton(tr("Show"), searchGroup);
    m_toggleSearchApiKeyButton->setFixedWidth(60);
    connect(m_toggleSearchApiKeyButton, &QPushButton::clicked, this, &ConfigurationWidget::toggleSearchApiKeyVisibility);
    searchApiKeyLayout->addWidget(m_toggleSearchApiKeyButton);

    searchFormLayout->addRow(tr("API Key:"), searchApiKeyLayout);

    m_searchReasoningEffortEdit = new QLineEdit(searchGroup);
    m_searchReasoningEffortEdit->setPlaceholderText(tr("e.g. low, medium, high (optional)"));
    searchFormLayout->addRow(tr("Reasoning Effort:"), m_searchReasoningEffortEdit);

    contentLayout->addWidget(searchGroup);

    // 3. Prompts Group
    auto *promptsGroup = new QGroupBox(tr("Prompts"), contentWidget);
    auto *promptsLayout = new QVBoxLayout(promptsGroup);

    auto *sysLabel = new QLabel(tr("<b>System Prompt</b>"), promptsGroup);
    promptsLayout->addWidget(sysLabel);
    m_systemPromptEdit = new QPlainTextEdit(promptsGroup);
    m_systemPromptEdit->setMinimumHeight(80);
    promptsLayout->addWidget(m_systemPromptEdit);

    auto *jjLabel = new QLabel(
        tr("<b>Job Judge Prompt</b> (requires <code>{targJD}</code>, <code>{myResume}</code>)"),
        promptsGroup
    );
    promptsLayout->addWidget(jjLabel);
    m_jobJudgePromptEdit = new QPlainTextEdit(promptsGroup);
    m_jobJudgePromptEdit->setMinimumHeight(120);
    promptsLayout->addWidget(m_jobJudgePromptEdit);

    auto *mmLabel = new QLabel(
        tr("<b>Make Materials Prompt</b> (requires <code>{myProfessionalTitle}</code>, <code>{myProfessionalSummary}</code>, <code>{myKeySkills}</code>, <code>{targJD}</code>, <code>{myResume}</code>, <code>{myTestimonials}</code>)"),
        promptsGroup
    );
    promptsLayout->addWidget(mmLabel);
    m_makeMaterialsPromptEdit = new QPlainTextEdit(promptsGroup);
    m_makeMaterialsPromptEdit->setMinimumHeight(120);
    promptsLayout->addWidget(m_makeMaterialsPromptEdit);

    contentLayout->addWidget(promptsGroup);

    // 4. Candidate Profile Group
    auto *candidateGroup = new QGroupBox(tr("Candidate Profile"), contentWidget);
    auto *candidateFormLayout = new QFormLayout(candidateGroup);
    candidateFormLayout->setLabelAlignment(Qt::AlignLeft);

    m_professionalTitleEdit = new QLineEdit(candidateGroup);
    candidateFormLayout->addRow(tr("Professional Title:"), m_professionalTitleEdit);

    m_professionalSummaryEdit = new QPlainTextEdit(candidateGroup);
    m_professionalSummaryEdit->setMinimumHeight(80);
    candidateFormLayout->addRow(tr("Professional Summary:"), m_professionalSummaryEdit);

    m_keySkillsEdit = new QPlainTextEdit(candidateGroup);
    m_keySkillsEdit->setMinimumHeight(80);
    candidateFormLayout->addRow(tr("Key Skills:"), m_keySkillsEdit);

    m_myResumeEdit = new QPlainTextEdit(candidateGroup);
    m_myResumeEdit->setMinimumHeight(140);
    candidateFormLayout->addRow(tr("My Resume:"), m_myResumeEdit);

    m_testimonialsEdit = new QPlainTextEdit(candidateGroup);
    m_testimonialsEdit->setMinimumHeight(80);
    candidateFormLayout->addRow(tr("Testimonials (optional):"), m_testimonialsEdit);

    contentLayout->addWidget(candidateGroup);

    // 5. Application Templates Group
    auto *templatesGroup = new QGroupBox(tr("Application Templates"), contentWidget);
    auto *templatesFormLayout = new QFormLayout(templatesGroup);
    templatesFormLayout->setLabelAlignment(Qt::AlignLeft);

    auto *resumeRow = new QHBoxLayout();
    m_resumeTemplateEdit = new QLineEdit(templatesGroup);
    resumeRow->addWidget(m_resumeTemplateEdit);
    m_browseResumeButton = new QPushButton(tr("Browse..."), templatesGroup);
    connect(m_browseResumeButton, &QPushButton::clicked, this, &ConfigurationWidget::onBrowseResumeTemplate);
    resumeRow->addWidget(m_browseResumeButton);
    templatesFormLayout->addRow(tr("Resume Template (.odt):"), resumeRow);

    auto *coverRow = new QHBoxLayout();
    m_coverTemplateEdit = new QLineEdit(templatesGroup);
    coverRow->addWidget(m_coverTemplateEdit);
    m_browseCoverButton = new QPushButton(tr("Browse..."), templatesGroup);
    connect(m_browseCoverButton, &QPushButton::clicked, this, &ConfigurationWidget::onBrowseCoverTemplate);
    coverRow->addWidget(m_browseCoverButton);
    templatesFormLayout->addRow(tr("Cover Letter Template (.odt):"), coverRow);

    contentLayout->addWidget(templatesGroup);

    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);

    // Bottom Action Row: Save and Status
    auto *actionRow = new QHBoxLayout();
    m_saveButton = new QPushButton(tr("Save Configuration"), this);
    m_saveButton->setStyleSheet(QStringLiteral("font-weight: bold; padding: 6px 14px;"));
    connect(m_saveButton, &QPushButton::clicked, this, &ConfigurationWidget::onSaveClicked);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(QStringLiteral("font-weight: bold; margin-left: 10px;"));

    actionRow->addWidget(m_saveButton);
    actionRow->addWidget(m_statusLabel);
    actionRow->addStretch();
    mainLayout->addLayout(actionRow);
}

void ConfigurationWidget::toggleApiKeyVisibility() {
    if (m_apiKeyEdit->echoMode() == QLineEdit::Password) {
        m_apiKeyEdit->setEchoMode(QLineEdit::Normal);
        m_toggleApiKeyButton->setText(tr("Hide"));
    } else {
        m_apiKeyEdit->setEchoMode(QLineEdit::Password);
        m_toggleApiKeyButton->setText(tr("Show"));
    }
}

void ConfigurationWidget::toggleSearchApiKeyVisibility() {
    if (m_searchApiKeyEdit->echoMode() == QLineEdit::Password) {
        m_searchApiKeyEdit->setEchoMode(QLineEdit::Normal);
        m_toggleSearchApiKeyButton->setText(tr("Hide"));
    } else {
        m_searchApiKeyEdit->setEchoMode(QLineEdit::Password);
        m_toggleSearchApiKeyButton->setText(tr("Show"));
    }
}

void ConfigurationWidget::onBrowseResumeTemplate() {
    const QString currentPath = m_resumeTemplateEdit->text().trimmed();
    const QString startDir = currentPath.isEmpty() ? ProjectPaths::templatesDir() : QFileInfo(currentPath).absolutePath();
    const QString selected = QFileDialog::getOpenFileName(
        this,
        tr("Select Resume ODT Template"),
        startDir,
        tr("OpenDocument Text (*.odt);;All Files (*)")
    );
    if (!selected.isEmpty()) {
        m_resumeTemplateEdit->setText(selected);
    }
}

void ConfigurationWidget::onBrowseCoverTemplate() {
    const QString currentPath = m_coverTemplateEdit->text().trimmed();
    const QString startDir = currentPath.isEmpty() ? ProjectPaths::templatesDir() : QFileInfo(currentPath).absolutePath();
    const QString selected = QFileDialog::getOpenFileName(
        this,
        tr("Select Cover Letter ODT Template"),
        startDir,
        tr("OpenDocument Text (*.odt);;All Files (*)")
    );
    if (!selected.isEmpty()) {
        m_coverTemplateEdit->setText(selected);
    }
}

void ConfigurationWidget::setConfig(const AppConfig &config) {
    m_version = config.version;
    m_endpointEdit->setText(config.apiEndpoint);
    m_modelEdit->setText(config.model);
    m_apiKeyEdit->setText(config.apiKey);
    m_reasoningEffortEdit->setText(config.reasoningEffort);
    m_timeoutSpinBox->setValue(config.timeoutSeconds > 0 ? config.timeoutSeconds : 300);

    m_searchEndpointEdit->setText(config.searchEndpoint);
    m_searchModelEdit->setText(config.searchModel);
    m_searchApiKeyEdit->setText(config.searchApiKey);
    m_searchReasoningEffortEdit->setText(config.searchReasoningEffort);

    m_systemPromptEdit->setPlainText(config.systemPrompt);
    m_jobJudgePromptEdit->setPlainText(config.jobJudgePrompt);
    m_makeMaterialsPromptEdit->setPlainText(config.makeMaterialsPrompt);

    m_professionalTitleEdit->setText(config.professionalTitle);
    m_professionalSummaryEdit->setPlainText(config.professionalSummary);
    m_keySkillsEdit->setPlainText(config.keySkills);
    m_myResumeEdit->setPlainText(config.myResume);
    m_testimonialsEdit->setPlainText(config.testimonials);

    m_resumeTemplateEdit->setText(config.resumeTemplatePath);
    m_coverTemplateEdit->setText(config.coverLetterTemplatePath);

    m_statusLabel->clear();
}

AppConfig ConfigurationWidget::config() const {
    AppConfig c;
    c.version = m_version;
    c.apiEndpoint = m_endpointEdit->text().trimmed();
    c.model = m_modelEdit->text().trimmed();
    c.apiKey = m_apiKeyEdit->text().trimmed();
    c.reasoningEffort = m_reasoningEffortEdit->text().trimmed();
    c.timeoutSeconds = m_timeoutSpinBox->value();

    c.searchEndpoint = m_searchEndpointEdit->text().trimmed();
    c.searchModel = m_searchModelEdit->text().trimmed();
    c.searchApiKey = m_searchApiKeyEdit->text().trimmed();
    c.searchReasoningEffort = m_searchReasoningEffortEdit->text().trimmed();
    c.searchTimeoutSeconds = c.timeoutSeconds;

    c.systemPrompt = m_systemPromptEdit->toPlainText();
    c.jobJudgePrompt = m_jobJudgePromptEdit->toPlainText();
    c.makeMaterialsPrompt = m_makeMaterialsPromptEdit->toPlainText();

    c.professionalTitle = m_professionalTitleEdit->text().trimmed();
    c.professionalSummary = m_professionalSummaryEdit->toPlainText().trimmed();
    c.keySkills = m_keySkillsEdit->toPlainText().trimmed();
    c.myResume = m_myResumeEdit->toPlainText();
    c.testimonials = m_testimonialsEdit->toPlainText();

    c.resumeTemplatePath = m_resumeTemplateEdit->text().trimmed();
    c.coverLetterTemplatePath = m_coverTemplateEdit->text().trimmed();

    return c;
}

void ConfigurationWidget::onSaveClicked() {
    AppConfig current = config();

    // Validate Job Judge Prompt placeholders
    QString jjError;
    if (!PromptRenderer::validateTemplate(current.jobJudgePrompt, &jjError)) {
        m_statusLabel->setStyleSheet(QStringLiteral("color: #dc3545; font-weight: bold;"));
        m_statusLabel->setText(jjError);
        return;
    }

    // Validate Make Materials Prompt placeholders
    QString mmError;
    if (!PromptRenderer::validateMakeMaterialsTemplate(current.makeMaterialsPrompt, &mmError)) {
        m_statusLabel->setStyleSheet(QStringLiteral("color: #dc3545; font-weight: bold;"));
        m_statusLabel->setText(mmError);
        return;
    }

    QString saveError;
    if (!m_configManager.save(current, &saveError)) {
        m_statusLabel->setStyleSheet(QStringLiteral("color: #dc3545; font-weight: bold;"));
        m_statusLabel->setText(QStringLiteral("Save failed: %1").arg(saveError));
        return;
    }

    m_statusLabel->setStyleSheet(QStringLiteral("color: #198754; font-weight: bold;"));
    m_statusLabel->setText(tr("Configuration saved successfully."));

    emit configSaved(current);
}

} // namespace jobstarr
