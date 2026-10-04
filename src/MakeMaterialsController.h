#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include "JobRecord.h"
#include "ConfigManager.h"
#include "CandidateProfile.h"
#include "MakeMaterialsResult.h"
#include "MakeMaterialsParser.h"
#include "CompanyAddressLookup.h"
#include "FilenameSanitizer.h"
#include "OdtTemplateRenderer.h"
#include "PdfConverter.h"
#include "OutputManager.h"
#include "LlmClient.h"

namespace jobstarr {

enum class MakeMaterialsState {
    Idle,
    Validating,
    GeneratingMaterials,
    ParsingMaterials,
    LookingUpCompanyAddress,
    RenderingResume,
    RenderingCoverLetter,
    ConvertingPdf,
    Publishing,
    Complete,
    Error
};

class MakeMaterialsController : public QObject {
    Q_OBJECT
public:
    explicit MakeMaterialsController(QNetworkAccessManager *networkManager = nullptr,
                                    std::shared_ptr<IPdfProcessRunner> pdfRunner = nullptr,
                                    QObject *parent = nullptr);
    ~MakeMaterialsController() override;

    bool isRunning() const;
    MakeMaterialsState currentState() const { return m_state; }

    bool runPreflightChecks(const JobRecord &job,
                            const AppConfig &config,
                            QString *errorMessage = nullptr);

    void start(const JobRecord &job, const AppConfig &config);
    void cancel();

signals:
    void started();
    void progress(int percent, const QString &statusText);
    void success(const MakeMaterialsResult &result, const QStringList &publishedFiles);
    void error(const QString &errorMessage);

private slots:
    void onPrimaryLlmSuccess(const QString &assistantContent);
    void onPrimaryLlmError(const QString &errorMessage);

    void onSearchLlmSuccess(const QString &assistantContent);
    void onSearchLlmError(const QString &errorMessage);

private:
    void setState(MakeMaterialsState state, int percent, const QString &statusText = QString());
    void fail(const QString &errorMessage);
    void renderAndPublish();

    MakeMaterialsState m_state{MakeMaterialsState::Idle};

    JobRecord m_job;
    AppConfig m_config;

    LlmClient *m_primaryLlmClient{nullptr};
    LlmClient *m_searchLlmClient{nullptr};
    PdfConverter m_pdfConverter;
    OutputManager m_outputManager;

    MakeMaterialsResult m_parsedResult;
    QString m_sanitizedResumeBasename;
    QString m_sanitizedCoverBasename;
    QString m_companyAddress;
    QString m_stagingDir;
};

} // namespace jobstarr
