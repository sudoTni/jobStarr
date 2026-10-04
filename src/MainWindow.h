#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QTabWidget>
#include <QTimer>
#include "ConfigManager.h"
#include "JobRecord.h"
#include "JobJudgeResult.h"
#include "JobSpyClient.h"
#include "LlmClient.h"
#include "ConfigurationWidget.h"
#include "MakeMaterialsController.h"

namespace jobstarr {

enum class AppState {
    Idle,
    Grabbing,
    JobReady,
    Judging,
    Judged,
    MakingMaterials,
    MaterialsReady,
    Error
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

    AppState currentState() const { return m_state; }

private slots:
    void onGrabClicked();
    void onJudgeClicked();
    void onMakeMaterialsClicked();
    void onOpenOutputFolderClicked();

    void onGrabStarted();
    void onGrabSuccess(const JobRecord &record);
    void onGrabError(const QString &errorMessage);

    void onJudgeStarted();
    void onJudgeSuccess(const JobJudgeResult &result);
    void onJudgeError(const QString &errorMessage);

    void onMakeMaterialsStarted();
    void onMakeMaterialsProgress(int percent, const QString &statusText);
    void onMakeMaterialsSuccess(const MakeMaterialsResult &result, const QStringList &publishedFiles);
    void onMakeMaterialsError(const QString &errorMessage);

    void onConfigSaved(const AppConfig &config);
    void onProgressTimerTick();

private:
    void setupUi();
    void setAppState(AppState state, const QString &statusText = QString());
    void clearJudgment();
    void clearApplicationPackage();
    void startProgress(const QString &operationName);
    void stopProgress();

    AppState m_state{AppState::Idle};
    AppConfig m_config;
    ConfigManager m_configManager;

    std::optional<JobRecord> m_currentJob;
    JobSpyClient *m_jobSpyClient{nullptr};
    LlmClient *m_llmClient{nullptr};
    MakeMaterialsController *m_materialsController{nullptr};

    // UI Widgets
    QTabWidget *m_tabWidget{nullptr};
    QWidget *m_jobTab{nullptr};
    ConfigurationWidget *m_configWidget{nullptr};

    QLineEdit *m_urlEdit{nullptr};
    QPushButton *m_grabButton{nullptr};
    QLabel *m_statusLabel{nullptr};
    QProgressBar *m_progressBar{nullptr};
    QTimer *m_progressTimer{nullptr};
    int m_elapsedSeconds{0};
    QString m_activeOperation;

    QPlainTextEdit *m_grabbedJobEdit{nullptr};
    QPushButton *m_judgeButton{nullptr};
    QPushButton *m_makeMaterialsButton{nullptr};

    QLabel *m_matchedLabel{nullptr};
    QLabel *m_confidenceLabel{nullptr};
    QPlainTextEdit *m_rationaleEdit{nullptr};

    // Application Package UI
    QLabel *m_packageStatusLabel{nullptr};
    QProgressBar *m_packageProgressBar{nullptr};
    QPlainTextEdit *m_packageDetailsEdit{nullptr};
    QPushButton *m_openFolderButton{nullptr};
};

} // namespace jobstarr
