#include "MakeMaterialsController.h"
#include "PromptRenderer.h"
#include <QDate>
#include <QLocale>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace jobstarr {

MakeMaterialsController::MakeMaterialsController(QNetworkAccessManager *networkManager,
                                               std::shared_ptr<IPdfProcessRunner> pdfRunner,
                                               QObject *parent)
    : QObject(parent),
      m_primaryLlmClient(new LlmClient(networkManager, this)),
      m_searchLlmClient(new LlmClient(networkManager, this)),
      m_pdfConverter(pdfRunner) {

    connect(m_primaryLlmClient, &LlmClient::chatSuccess, this, &MakeMaterialsController::onPrimaryLlmSuccess);
    connect(m_primaryLlmClient, &LlmClient::chatError, this, &MakeMaterialsController::onPrimaryLlmError);

    connect(m_searchLlmClient, &LlmClient::chatSuccess, this, &MakeMaterialsController::onSearchLlmSuccess);
    connect(m_searchLlmClient, &LlmClient::chatError, this, &MakeMaterialsController::onSearchLlmError);
}

MakeMaterialsController::~MakeMaterialsController() {
    cancel();
}

bool MakeMaterialsController::isRunning() const {
    return m_state != MakeMaterialsState::Idle &&
           m_state != MakeMaterialsState::Complete &&
           m_state != MakeMaterialsState::Error;
}

void MakeMaterialsController::setState(MakeMaterialsState state, int percent, const QString &statusText) {
    m_state = state;
    emit progress(percent, statusText);
}

void MakeMaterialsController::fail(const QString &errorMessage) {
    if (!m_stagingDir.isEmpty()) {
        m_outputManager.cleanStagingDirectory(m_stagingDir);
        m_stagingDir.clear();
    }
    m_primaryLlmClient->cancel();
    m_searchLlmClient->cancel();
    m_state = MakeMaterialsState::Error;
    emit error(errorMessage);
}

void MakeMaterialsController::cancel() {
    if (!m_stagingDir.isEmpty()) {
        m_outputManager.cleanStagingDirectory(m_stagingDir);
        m_stagingDir.clear();
    }
    if (m_primaryLlmClient) m_primaryLlmClient->cancel();
    if (m_searchLlmClient) m_searchLlmClient->cancel();
    m_state = MakeMaterialsState::Idle;
}

bool MakeMaterialsController::runPreflightChecks(const JobRecord &job,
                                                const AppConfig &config,
                                                QString *errorMessage) {
    if (!job.isValid()) {
        if (errorMessage) *errorMessage = QStringLiteral("A valid Job Record is required to make application package.");
        return false;
    }

    if (config.apiEndpoint.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Primary LLM Endpoint is not configured.");
        return false;
    }
    if (config.model.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Primary LLM Model is not configured.");
        return false;
    }
    if (config.apiKey.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Primary LLM API Key is not configured.");
        return false;
    }

    if (config.searchEndpoint.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Search LLM Endpoint is not configured.");
        return false;
    }
    if (config.searchModel.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Search LLM Model is not configured.");
        return false;
    }
    if (config.searchApiKey.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Search LLM API Key is not configured.");
        return false;
    }

    if (config.systemPrompt.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("System Prompt is empty.");
        return false;
    }

    if (config.makeMaterialsPrompt.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Make Materials Prompt is empty.");
        return false;
    }

    QString promptErr;
    if (!PromptRenderer::validateMakeMaterialsTemplate(config.makeMaterialsPrompt, &promptErr)) {
        if (errorMessage) *errorMessage = promptErr;
        return false;
    }

    CandidateProfile candidate;
    candidate.professionalTitle = config.professionalTitle;
    candidate.professionalSummary = config.professionalSummary;
    candidate.keySkills = config.keySkills;
    candidate.resume = config.myResume;
    candidate.testimonials = config.testimonials;

    QString candidateErr;
    if (!candidate.isValid(&candidateErr)) {
        if (errorMessage) *errorMessage = candidateErr;
        return false;
    }

    if (config.resumeTemplatePath.trimmed().isEmpty() || !QFile::exists(config.resumeTemplatePath)) {
        if (errorMessage) *errorMessage = QStringLiteral("Resume template file does not exist: %1").arg(config.resumeTemplatePath);
        return false;
    }
    if (config.coverLetterTemplatePath.trimmed().isEmpty() || !QFile::exists(config.coverLetterTemplatePath)) {
        if (errorMessage) *errorMessage = QStringLiteral("Cover letter template file does not exist: %1").arg(config.coverLetterTemplatePath);
        return false;
    }

    QString templateErr;
    if (!OdtTemplateRenderer::validateResumeTemplate(config.resumeTemplatePath, &templateErr)) {
        if (errorMessage) *errorMessage = QStringLiteral("Resume template validation failed: %1").arg(templateErr);
        return false;
    }
    if (!OdtTemplateRenderer::validateCoverTemplate(config.coverLetterTemplatePath, &templateErr)) {
        if (errorMessage) *errorMessage = QStringLiteral("Cover letter template validation failed: %1").arg(templateErr);
        return false;
    }

    if (PdfConverter::findConverterExecutable().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PDF conversion requires LibreOffice. "
                                           "Install LibreOffice or make 'libreoffice'/'soffice' available in PATH.");
        }
        return false;
    }

    const QString outDir = m_outputManager.outputDirectory();
    if (!QDir().mkpath(outDir)) {
        if (errorMessage) *errorMessage = QStringLiteral("Failed to access or create output directory: %1").arg(outDir);
        return false;
    }

    return true;
}

void MakeMaterialsController::start(const JobRecord &job, const AppConfig &config) {
    if (isRunning()) {
        emit error(QStringLiteral("An application package generation is already in progress."));
        return;
    }

    emit started();
    setState(MakeMaterialsState::Validating, 0, QStringLiteral("Preparing application package..."));

    // Snapshot job and config
    m_job = job;
    m_config = config;

    QString preflightErr;
    if (!runPreflightChecks(m_job, m_config, &preflightErr)) {
        fail(QStringLiteral("Preflight check failed: %1").arg(preflightErr));
        return;
    }

    setState(MakeMaterialsState::GeneratingMaterials, 20, QStringLiteral("Generating tailored materials..."));

    // Normalize candidate key skills
    const QString normalizedSkills = CandidateProfile::normalizeKeySkills(m_config.keySkills);

    // Render Make Materials prompt
    QString renderErr;
    const QString userPrompt = PromptRenderer::renderMakeMaterialsPrompt(
        m_config.makeMaterialsPrompt,
        m_config.professionalTitle,
        m_config.professionalSummary,
        normalizedSkills,
        m_job.toJsonString(true),
        m_config.myResume,
        m_config.testimonials,
        &renderErr
    );

    if (!renderErr.isEmpty()) {
        fail(QStringLiteral("Prompt rendering failed: %1").arg(renderErr));
        return;
    }

    const int timeoutMs = (m_config.timeoutSeconds > 0 ? m_config.timeoutSeconds : 300) * 1000;
    m_primaryLlmClient->sendChatCompletion(
        m_config.apiEndpoint,
        m_config.model,
        m_config.apiKey,
        m_config.systemPrompt,
        userPrompt,
        timeoutMs,
        m_config.reasoningEffort
    );
}

void MakeMaterialsController::onPrimaryLlmSuccess(const QString &assistantContent) {
    setState(MakeMaterialsState::ParsingMaterials, 40, QStringLiteral("Parsing generated materials..."));

    QString parseErr;
    auto parsedOpt = MakeMaterialsParser::parse(assistantContent, &parseErr);
    if (!parsedOpt.has_value()) {
        fail(QStringLiteral("Application package failed while parsing LLM response: %1").arg(parseErr));
        return;
    }

    m_parsedResult = parsedOpt.value();

    // Sanitize basenames
    QString resSanitizeErr;
    auto resumeBasenameOpt = FilenameSanitizer::sanitize(m_parsedResult.resumeFilename, &resSanitizeErr);
    if (!resumeBasenameOpt.has_value()) {
        fail(QStringLiteral("Resume filename is invalid: %1").arg(resSanitizeErr));
        return;
    }
    m_sanitizedResumeBasename = resumeBasenameOpt.value();
    m_parsedResult.resumeFilename = m_sanitizedResumeBasename;

    QString covSanitizeErr;
    auto coverBasenameOpt = FilenameSanitizer::sanitize(m_parsedResult.coverLetterFilename, &covSanitizeErr);
    if (!coverBasenameOpt.has_value()) {
        fail(QStringLiteral("Cover letter filename is invalid: %1").arg(covSanitizeErr));
        return;
    }
    m_sanitizedCoverBasename = coverBasenameOpt.value();
    m_parsedResult.coverLetterFilename = m_sanitizedCoverBasename;

    // Next stage: Search LLM lookup for company address
    setState(MakeMaterialsState::LookingUpCompanyAddress, 50, QStringLiteral("Finding company address..."));

    const QString searchPrompt = CompanyAddressLookup::buildUserPrompt(m_job);
    const int searchTimeoutMs = (m_config.searchTimeoutSeconds > 0 ? m_config.searchTimeoutSeconds : 300) * 1000;

    m_searchLlmClient->sendChatCompletion(
        m_config.searchEndpoint,
        m_config.searchModel,
        m_config.searchApiKey,
        m_config.systemPrompt,
        searchPrompt,
        searchTimeoutMs,
        m_config.searchReasoningEffort
    );
}

void MakeMaterialsController::onPrimaryLlmError(const QString &errorMessage) {
    fail(QStringLiteral("Primary LLM request failed: %1").arg(errorMessage));
}

void MakeMaterialsController::onSearchLlmSuccess(const QString &assistantContent) {
    QString addrErr;
    auto addressOpt = CompanyAddressLookup::parseResponse(assistantContent, &addrErr);
    if (!addressOpt.has_value()) {
        fail(QStringLiteral("Application package failed while finding company address: %1").arg(addrErr));
        return;
    }

    m_companyAddress = addressOpt.value();
    renderAndPublish();
}

void MakeMaterialsController::onSearchLlmError(const QString &errorMessage) {
    fail(QStringLiteral("Company address lookup failed: %1").arg(errorMessage));
}

void MakeMaterialsController::renderAndPublish() {
    // 1. Calculate today's date locally in English (Section 25)
    QLocale enUS(QLocale::English, QLocale::UnitedStates);
    const QString todaysDate = enUS.toString(QDate::currentDate(), QStringLiteral("MMMM d, yyyy"));

    // 2. Create staging directory
    QString stagingErr;
    m_stagingDir = m_outputManager.createStagingDirectory(&stagingErr);
    if (m_stagingDir.isEmpty()) {
        fail(QStringLiteral("Failed to create staging directory: %1").arg(stagingErr));
        return;
    }

    // 3. Render resume ODT
    setState(MakeMaterialsState::RenderingResume, 65, QStringLiteral("Rendering resume..."));
    const QString stagedResumeOdt = QDir(m_stagingDir).filePath(m_sanitizedResumeBasename + QStringLiteral(".odt"));
    QString resumeRenderErr;
    if (!OdtTemplateRenderer::renderResume(m_config.resumeTemplatePath,
                                          stagedResumeOdt,
                                          m_parsedResult.professionalTitle,
                                          m_parsedResult.professionalSummary,
                                          m_parsedResult.optimizedSkills,
                                          &resumeRenderErr)) {
        fail(QStringLiteral("Application package failed while rendering resume: %1").arg(resumeRenderErr));
        return;
    }

    // 4. Render cover letter ODT
    setState(MakeMaterialsState::RenderingCoverLetter, 75, QStringLiteral("Rendering cover letter..."));
    const QString stagedCoverOdt = QDir(m_stagingDir).filePath(m_sanitizedCoverBasename + QStringLiteral(".odt"));
    QString coverRenderErr;
    if (!OdtTemplateRenderer::renderCoverLetter(m_config.coverLetterTemplatePath,
                                               stagedCoverOdt,
                                               m_parsedResult.professionalTitle,
                                               todaysDate,
                                               m_companyAddress,
                                               m_job.companyName(),
                                               m_parsedResult.coverLetterBody,
                                               &coverRenderErr)) {
        fail(QStringLiteral("Application package failed while rendering cover letter: %1").arg(coverRenderErr));
        return;
    }

    // 5. Convert documents to PDF
    setState(MakeMaterialsState::ConvertingPdf, 85, QStringLiteral("Converting documents to PDF..."));

    QString pdfErr;
    QString resumePdfPath;
    if (!m_pdfConverter.convertToPdf(stagedResumeOdt, m_stagingDir, &resumePdfPath, &pdfErr)) {
        fail(QStringLiteral("Application package failed during resume PDF conversion: %1").arg(pdfErr));
        return;
    }

    QString coverPdfPath;
    if (!m_pdfConverter.convertToPdf(stagedCoverOdt, m_stagingDir, &coverPdfPath, &pdfErr)) {
        fail(QStringLiteral("Application package failed during cover letter PDF conversion: %1").arg(pdfErr));
        return;
    }

    // 6. Transactional publish
    setState(MakeMaterialsState::Publishing, 95, QStringLiteral("Publishing application package..."));
    QStringList publishedFiles;
    QString pubErr;
    if (!m_outputManager.publishStagedArtifacts(m_stagingDir,
                                               m_sanitizedResumeBasename,
                                               m_sanitizedCoverBasename,
                                               &publishedFiles,
                                               &pubErr)) {
        fail(QStringLiteral("Application package failed while publishing artifacts: %1").arg(pubErr));
        return;
    }

    m_stagingDir.clear();
    setState(MakeMaterialsState::Complete, 100, QStringLiteral("Application package ready"));
    emit success(m_parsedResult, publishedFiles);
}

} // namespace jobstarr
