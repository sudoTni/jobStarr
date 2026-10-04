#include "MainWindow.h"
#include "UrlValidator.h"
#include "PromptRenderer.h"
#include "OutputManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QMessageBox>
#include <QFontDatabase>

namespace jobstarr {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_jobSpyClient(new JobSpyClient(this)),
      m_llmClient(new LlmClient(nullptr, this)),
      m_materialsController(new MakeMaterialsController(nullptr, nullptr, this)),
      m_progressTimer(new QTimer(this)) {
    setWindowTitle(QStringLiteral("jobStarr"));
    setWindowIcon(QIcon(QStringLiteral(":/icon/jobStarr_icon.png")));
    resize(950, 850);

    setupUi();

    m_progressTimer->setInterval(1000);
    connect(m_progressTimer, &QTimer::timeout, this, &MainWindow::onProgressTimerTick);

    // Load configuration
    QString configError;
    if (!m_configManager.load(m_config, &configError)) {
        setAppState(AppState::Error, QStringLiteral("Configuration load error: %1").arg(configError));
    }
    m_configWidget->setConfig(m_config);

    // JobSpy client signals
    connect(m_jobSpyClient, &JobSpyClient::grabStarted, this, &MainWindow::onGrabStarted);
    connect(m_jobSpyClient, &JobSpyClient::grabSuccess, this, &MainWindow::onGrabSuccess);
    connect(m_jobSpyClient, &JobSpyClient::grabError, this, &MainWindow::onGrabError);

    // LLM client signals
    connect(m_llmClient, &LlmClient::judgeStarted, this, &MainWindow::onJudgeStarted);
    connect(m_llmClient, &LlmClient::judgeSuccess, this, &MainWindow::onJudgeSuccess);
    connect(m_llmClient, &LlmClient::judgeError, this, &MainWindow::onJudgeError);

    // Make Materials controller signals
    connect(m_materialsController, &MakeMaterialsController::started, this, &MainWindow::onMakeMaterialsStarted);
    connect(m_materialsController, &MakeMaterialsController::progress, this, &MainWindow::onMakeMaterialsProgress);
    connect(m_materialsController, &MakeMaterialsController::success, this, &MainWindow::onMakeMaterialsSuccess);
    connect(m_materialsController, &MakeMaterialsController::error, this, &MainWindow::onMakeMaterialsError);

    connect(m_configWidget, &ConfigurationWidget::configSaved, this, &MainWindow::onConfigSaved);

    setAppState(AppState::Idle, QStringLiteral("Ready"));
}

void MainWindow::setupUi() {
    auto *centralWidget = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(8, 8, 8, 8);

    m_tabWidget = new QTabWidget(centralWidget);

    // --- Tab 1: Job View ---
    m_jobTab = new QWidget(m_tabWidget);
    auto *jobLayout = new QVBoxLayout(m_jobTab);
    jobLayout->setSpacing(6);

    // URL row: "Job URL: [______] [Grab]"
    auto *urlRow = new QHBoxLayout();
    auto *urlLabel = new QLabel(tr("Job URL:"), m_jobTab);
    urlLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_urlEdit = new QLineEdit(m_jobTab);
    m_urlEdit->setPlaceholderText(tr("Enter LinkedIn or Indeed job URL (e.g. https://www.linkedin.com/jobs/view/...)"));

    m_grabButton = new QPushButton(tr("Grab"), m_jobTab);
    m_grabButton->setFixedWidth(90);
    m_grabButton->setStyleSheet(QStringLiteral("font-weight: bold; padding: 5px;"));
    connect(m_grabButton, &QPushButton::clicked, this, &MainWindow::onGrabClicked);

    urlRow->addWidget(urlLabel);
    urlRow->addWidget(m_urlEdit);
    urlRow->addWidget(m_grabButton);
    jobLayout->addLayout(urlRow);

    // Status label: "Status: Ready"
    m_statusLabel = new QLabel(tr("Status: Ready"), m_jobTab);
    m_statusLabel->setStyleSheet(QStringLiteral("font-weight: bold; margin-top: 2px; margin-bottom: 2px;"));
    jobLayout->addWidget(m_statusLabel);

    // Progress bar for visual progress during Grab / Judge / Make Materials
    m_progressBar = new QProgressBar(m_jobTab);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    m_progressBar->setStyleSheet(QStringLiteral(
        "QProgressBar { border: 1px solid #ced4da; border-radius: 3px; background-color: #e9ecef; }"
        "QProgressBar::chunk { background-color: #0d6efd; border-radius: 2px; }"
    ));
    m_progressBar->setVisible(false);
    jobLayout->addWidget(m_progressBar);

    // Grabbed Job section
    auto *grabbedJobLabel = new QLabel(tr("Grabbed Job"), m_jobTab);
    grabbedJobLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    jobLayout->addWidget(grabbedJobLabel);

    m_grabbedJobEdit = new QPlainTextEdit(m_jobTab);
    m_grabbedJobEdit->setReadOnly(true);
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_grabbedJobEdit->setFont(monoFont);
    m_grabbedJobEdit->setMinimumHeight(150);
    jobLayout->addWidget(m_grabbedJobEdit);

    // Action button row: [Judge] [Make Application Package]
    auto *actionRow = new QHBoxLayout();
    m_judgeButton = new QPushButton(tr("Judge"), m_jobTab);
    m_judgeButton->setFixedWidth(120);
    m_judgeButton->setEnabled(false);
    m_judgeButton->setStyleSheet(QStringLiteral("font-weight: bold; padding: 6px;"));
    connect(m_judgeButton, &QPushButton::clicked, this, &MainWindow::onJudgeClicked);
    actionRow->addWidget(m_judgeButton);

    m_makeMaterialsButton = new QPushButton(tr("Make Application Package"), m_jobTab);
    m_makeMaterialsButton->setMinimumWidth(210);
    m_makeMaterialsButton->setEnabled(false);
    m_makeMaterialsButton->setStyleSheet(QStringLiteral("font-weight: bold; padding: 6px;"));
    connect(m_makeMaterialsButton, &QPushButton::clicked, this, &MainWindow::onMakeMaterialsClicked);
    actionRow->addWidget(m_makeMaterialsButton);

    actionRow->addStretch();
    jobLayout->addLayout(actionRow);

    // Judgment results section
    auto *resultsLayout = new QHBoxLayout();
    m_matchedLabel = new QLabel(tr("Highly Matched: -"), m_jobTab);
    m_matchedLabel->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: bold;"));

    m_confidenceLabel = new QLabel(tr("Confidence: -"), m_jobTab);
    m_confidenceLabel->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: bold; margin-left: 20px;"));

    resultsLayout->addWidget(m_matchedLabel);
    resultsLayout->addWidget(m_confidenceLabel);
    resultsLayout->addStretch();
    jobLayout->addLayout(resultsLayout);

    auto *rationaleLabel = new QLabel(tr("Rationale:"), m_jobTab);
    rationaleLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    jobLayout->addWidget(rationaleLabel);

    m_rationaleEdit = new QPlainTextEdit(m_jobTab);
    m_rationaleEdit->setReadOnly(true);
    m_rationaleEdit->setMinimumHeight(80);
    m_rationaleEdit->setMaximumHeight(120);
    jobLayout->addWidget(m_rationaleEdit);

    // Application Package section
    auto *packageHeader = new QLabel(tr("Application Package:"), m_jobTab);
    packageHeader->setStyleSheet(QStringLiteral("font-weight: bold; margin-top: 4px;"));
    jobLayout->addWidget(packageHeader);

    m_packageStatusLabel = new QLabel(tr("Status: Ready"), m_jobTab);
    m_packageStatusLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #495057;"));
    jobLayout->addWidget(m_packageStatusLabel);

    m_packageProgressBar = new QProgressBar(m_jobTab);
    m_packageProgressBar->setRange(0, 100);
    m_packageProgressBar->setValue(0);
    m_packageProgressBar->setFixedHeight(8);
    m_packageProgressBar->setTextVisible(false);
    jobLayout->addWidget(m_packageProgressBar);

    m_packageDetailsEdit = new QPlainTextEdit(m_jobTab);
    m_packageDetailsEdit->setReadOnly(true);
    m_packageDetailsEdit->setMinimumHeight(70);
    m_packageDetailsEdit->setMaximumHeight(110);
    jobLayout->addWidget(m_packageDetailsEdit);

    auto *folderRow = new QHBoxLayout();
    m_openFolderButton = new QPushButton(tr("Open Output Folder"), m_jobTab);
    m_openFolderButton->setFixedWidth(180);
    m_openFolderButton->setEnabled(false);
    m_openFolderButton->setStyleSheet(QStringLiteral("font-weight: bold; padding: 5px;"));
    connect(m_openFolderButton, &QPushButton::clicked, this, &MainWindow::onOpenOutputFolderClicked);
    folderRow->addWidget(m_openFolderButton);
    folderRow->addStretch();
    jobLayout->addLayout(folderRow);

    m_tabWidget->addTab(m_jobTab, tr("Job"));

    // --- Tab 2: Configuration View ---
    m_configWidget = new ConfigurationWidget(m_tabWidget);
    m_tabWidget->addTab(m_configWidget, tr("Configuration"));

    rootLayout->addWidget(m_tabWidget);
    setCentralWidget(centralWidget);
}

void MainWindow::setAppState(AppState state, const QString &statusText) {
    m_state = state;

    if (!statusText.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("Status: %1").arg(statusText));
    }

    const bool hasJob = m_currentJob.has_value() && m_currentJob->isValid();

    switch (m_state) {
        case AppState::Idle:
            m_grabButton->setEnabled(true);
            m_judgeButton->setEnabled(hasJob);
            m_makeMaterialsButton->setEnabled(hasJob);
            break;
        case AppState::Grabbing:
            m_grabButton->setEnabled(false);
            m_judgeButton->setEnabled(false);
            m_makeMaterialsButton->setEnabled(false);
            break;
        case AppState::JobReady:
            m_grabButton->setEnabled(true);
            m_judgeButton->setEnabled(true);
            m_makeMaterialsButton->setEnabled(true);
            break;
        case AppState::Judging:
            m_grabButton->setEnabled(true);
            m_judgeButton->setEnabled(false);
            m_makeMaterialsButton->setEnabled(false);
            break;
        case AppState::Judged:
            m_grabButton->setEnabled(true);
            m_judgeButton->setEnabled(hasJob);
            m_makeMaterialsButton->setEnabled(hasJob);
            break;
        case AppState::MakingMaterials:
            m_grabButton->setEnabled(false);
            m_judgeButton->setEnabled(false);
            m_makeMaterialsButton->setEnabled(false);
            break;
        case AppState::MaterialsReady:
            m_grabButton->setEnabled(true);
            m_judgeButton->setEnabled(hasJob);
            m_makeMaterialsButton->setEnabled(hasJob);
            break;
        case AppState::Error:
            m_grabButton->setEnabled(true);
            m_judgeButton->setEnabled(hasJob);
            m_makeMaterialsButton->setEnabled(hasJob);
            break;
    }
}

void MainWindow::clearJudgment() {
    m_matchedLabel->setText(tr("Highly Matched: -"));
    m_matchedLabel->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: bold;"));
    m_confidenceLabel->setText(tr("Confidence: -"));
    m_rationaleEdit->clear();
}

void MainWindow::clearApplicationPackage() {
    m_packageStatusLabel->setText(tr("Status: Ready"));
    m_packageProgressBar->setValue(0);
    m_packageDetailsEdit->clear();
    m_openFolderButton->setEnabled(false);
}

void MainWindow::startProgress(const QString &operationName) {
    m_activeOperation = operationName;
    m_elapsedSeconds = 0;
    m_progressBar->setRange(0, 0); // Indeterminate animated pulsing
    m_progressBar->setVisible(true);
    m_statusLabel->setText(QStringLiteral("Status: %1... (0s)").arg(m_activeOperation));
    m_progressTimer->start(1000);
}

void MainWindow::stopProgress() {
    m_progressTimer->stop();
    m_progressBar->setVisible(false);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
}

void MainWindow::onProgressTimerTick() {
    m_elapsedSeconds++;
    m_statusLabel->setText(QStringLiteral("Status: %1... (%2s)").arg(m_activeOperation).arg(m_elapsedSeconds));
}

void MainWindow::onGrabClicked() {
    const QString url = m_urlEdit->text().trimmed();

    QString valError;
    if (!UrlValidator::isValidJobUrl(url, &valError)) {
        stopProgress();
        setAppState(AppState::Error, QStringLiteral("Error: %1").arg(valError));
        return;
    }

    // Reset current job and clear prior results
    m_currentJob.reset();
    m_grabbedJobEdit->clear();
    clearJudgment();
    clearApplicationPackage();

    m_jobSpyClient->grabJob(url);
}

void MainWindow::onGrabStarted() {
    startProgress(QStringLiteral("Grabbing job"));
    setAppState(AppState::Grabbing);
}

void MainWindow::onGrabSuccess(const JobRecord &record) {
    const int elapsed = m_elapsedSeconds;
    stopProgress();
    m_currentJob = record;
    m_grabbedJobEdit->setPlainText(record.toJsonString(true));
    clearJudgment();
    clearApplicationPackage();
    setAppState(AppState::JobReady, QStringLiteral("Job grabbed (took %1s)").arg(elapsed));
}

void MainWindow::onGrabError(const QString &errorMessage) {
    stopProgress();
    m_currentJob.reset();
    m_grabbedJobEdit->clear();
    clearJudgment();
    clearApplicationPackage();
    setAppState(AppState::Error, QStringLiteral("Error: %1").arg(errorMessage));
}

void MainWindow::onJudgeClicked() {
    if (!m_currentJob.has_value() || !m_currentJob->isValid()) {
        stopProgress();
        setAppState(AppState::Error, QStringLiteral("Error: No valid job grabbed to judge."));
        return;
    }

    // Read current config (Configuration page values are authoritative at runtime)
    m_config = m_configWidget->config();

    if (m_config.apiEndpoint.trimmed().isEmpty()) {
        stopProgress();
        setAppState(AppState::Error, QStringLiteral("Error: API Endpoint is not configured."));
        m_tabWidget->setCurrentWidget(m_configWidget);
        return;
    }

    if (m_config.model.trimmed().isEmpty()) {
        stopProgress();
        setAppState(AppState::Error, QStringLiteral("Error: Model is not configured."));
        m_tabWidget->setCurrentWidget(m_configWidget);
        return;
    }

    if (m_config.apiKey.trimmed().isEmpty()) {
        stopProgress();
        setAppState(AppState::Error, QStringLiteral("Error: API Key is not configured."));
        m_tabWidget->setCurrentWidget(m_configWidget);
        return;
    }

    QString renderErr;
    const QString serializedJob = m_currentJob->toJsonString(true);
    const QString userPrompt = PromptRenderer::render(m_config.jobJudgePrompt,
                                                      serializedJob,
                                                      m_config.myResume,
                                                      &renderErr);
    if (!renderErr.isEmpty()) {
        stopProgress();
        setAppState(AppState::Error, QStringLiteral("Error: %1").arg(renderErr));
        m_tabWidget->setCurrentWidget(m_configWidget);
        return;
    }

    clearJudgment();
    const int timeoutMs = (m_config.timeoutSeconds > 0 ? m_config.timeoutSeconds : 300) * 1000;
    m_llmClient->judgeJob(m_config.apiEndpoint,
                          m_config.model,
                          m_config.apiKey,
                          m_config.systemPrompt,
                          userPrompt,
                          timeoutMs,
                          m_config.reasoningEffort);
}

void MainWindow::onJudgeStarted() {
    startProgress(QStringLiteral("Judging"));
    setAppState(AppState::Judging);
}

void MainWindow::onJudgeSuccess(const JobJudgeResult &result) {
    const int elapsed = m_elapsedSeconds;
    stopProgress();
    setAppState(AppState::Judged, QStringLiteral("Judgment complete (took %1s)").arg(elapsed));

    if (result.isVeryHighlyAligned) {
        m_matchedLabel->setText(tr("Highly Matched: True"));
        m_matchedLabel->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: bold; color: #198754;"));
    } else {
        m_matchedLabel->setText(tr("Highly Matched: False"));
        m_matchedLabel->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: bold; color: #dc3545;"));
    }

    m_confidenceLabel->setText(tr("Confidence: %1").arg(QString::number(result.confidence, 'f', 2)));
    m_rationaleEdit->setPlainText(result.rationale);
}

void MainWindow::onJudgeError(const QString &errorMessage) {
    stopProgress();
    setAppState(AppState::Error, QStringLiteral("Error: %1").arg(errorMessage));
}

void MainWindow::onMakeMaterialsClicked() {
    if (!m_currentJob.has_value() || !m_currentJob->isValid()) {
        setAppState(AppState::Error, QStringLiteral("Error: No valid job grabbed to make application package."));
        return;
    }

    m_config = m_configWidget->config();

    QString preflightErr;
    if (!m_materialsController->runPreflightChecks(*m_currentJob, m_config, &preflightErr)) {
        setAppState(AppState::Error, QStringLiteral("Preflight error: %1").arg(preflightErr));
        m_packageStatusLabel->setText(QStringLiteral("Status: Preflight error: %1").arg(preflightErr));
        m_tabWidget->setCurrentWidget(m_configWidget);
        return;
    }

    clearApplicationPackage();
    m_materialsController->start(*m_currentJob, m_config);
}

void MainWindow::onMakeMaterialsStarted() {
    startProgress(QStringLiteral("Making Application Package"));
    setAppState(AppState::MakingMaterials);
}

void MainWindow::onMakeMaterialsProgress(int percent, const QString &statusText) {
    m_packageProgressBar->setValue(percent);
    m_packageStatusLabel->setText(QStringLiteral("Status: %1 (%2%)").arg(statusText).arg(percent));
}

void MainWindow::onMakeMaterialsSuccess(const MakeMaterialsResult &result, const QStringList &/*publishedFiles*/) {
    const int elapsed = m_elapsedSeconds;
    stopProgress();
    m_packageProgressBar->setValue(100);
    m_packageStatusLabel->setText(QStringLiteral("Status: Application package ready (took %1s)").arg(elapsed));
    m_openFolderButton->setEnabled(true);

    QString details = QStringLiteral(
        "Application package ready.\n\n"
        "Resume:\n"
        "%1.odt\n"
        "%1.pdf\n\n"
        "Cover Letter:\n"
        "%2.odt\n"
        "%2.pdf"
    ).arg(result.resumeFilename, result.coverLetterFilename);

    m_packageDetailsEdit->setPlainText(details);
    setAppState(AppState::MaterialsReady, QStringLiteral("Application package ready"));
}

void MainWindow::onMakeMaterialsError(const QString &errorMessage) {
    stopProgress();
    m_packageStatusLabel->setText(QStringLiteral("Status: %1").arg(errorMessage));
    setAppState(AppState::Error, QStringLiteral("Error: %1").arg(errorMessage));
}

void MainWindow::onOpenOutputFolderClicked() {
    OutputManager outMgr;
    QString err;
    if (!outMgr.openOutputFolder(&err)) {
        QMessageBox::warning(this, tr("Open Output Folder"), err);
    }
}

void MainWindow::onConfigSaved(const AppConfig &config) {
    m_config = config;
}

} // namespace jobstarr
