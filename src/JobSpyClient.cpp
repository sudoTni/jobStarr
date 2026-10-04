#include "JobSpyClient.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace jobstarr {

JobSpyClient::JobSpyClient(QObject *parent)
    : QObject(parent),
      m_process(new QProcess(this)),
      m_timer(new QTimer(this)) {
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &JobSpyClient::onTimeout);

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        m_stdoutBuffer.append(m_process->readAllStandardOutput());
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        m_stderrBuffer.append(m_process->readAllStandardError());
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &JobSpyClient::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &JobSpyClient::onProcessError);
}

JobSpyClient::~JobSpyClient() {
    cancel();
}

QString JobSpyClient::findBridgeScript() {
    // 1. Environment variable override
    const QString envBridge = QString::fromLocal8Bit(qgetenv("JOBSTARR_BRIDGE_PATH"));
    if (!envBridge.isEmpty() && QFileInfo::exists(envBridge)) {
        return envBridge;
    }

    // 2. Next to executable
    const QString appDir = QCoreApplication::applicationDirPath();
    QString candidate = appDir + QLatin1String("/jobspy_bridge.py");
    if (QFileInfo::exists(candidate)) return candidate;

    // 3. In tools subfolder next to executable
    candidate = appDir + QLatin1String("/tools/jobspy_bridge.py");
    if (QFileInfo::exists(candidate)) return candidate;

#ifdef JOBSTARR_SOURCE_DIR
    candidate = QStringLiteral(JOBSTARR_SOURCE_DIR) + QLatin1String("/tools/jobspy_bridge.py");
    if (QFileInfo::exists(candidate)) return candidate;
#endif

    return QString();
}

bool JobSpyClient::isRunning() const {
    return m_process->state() != QProcess::NotRunning;
}

void JobSpyClient::cancel() {
    m_timer->stop();
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void JobSpyClient::grabJob(const QString &url, int timeoutMs) {
    if (isRunning()) {
        emit grabError(QStringLiteral("A job grabbing operation is already in progress."));
        return;
    }

    const QString bridgeScript = findBridgeScript();
    if (bridgeScript.isEmpty()) {
        emit grabError(QStringLiteral("JobSpy bridge script (jobspy_bridge.py) could not be found."));
        return;
    }

    const QString pythonExe = QStandardPaths::findExecutable(QStringLiteral("python3"));
    if (pythonExe.isEmpty()) {
        emit grabError(QStringLiteral("Python 3 executable ('python3') was not found on system PATH."));
        return;
    }

    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
    m_timedOut = false;

    // Arguments passed separately to prevent shell injection
    const QStringList arguments{bridgeScript, QStringLiteral("--url"), url};

    emit grabStarted();

    m_timer->start(timeoutMs);
    m_process->start(pythonExe, arguments);
}

void JobSpyClient::onTimeout() {
    if (isRunning()) {
        m_timedOut = true;
        m_process->kill();
        emit grabError(QStringLiteral("Job grabbing operation timed out."));
    }
}

void JobSpyClient::onProcessError(QProcess::ProcessError error) {
    m_timer->stop();
    if (m_timedOut) return;

    if (error == QProcess::FailedToStart) {
        emit grabError(QStringLiteral("Failed to start JobSpy process."));
    } else if (error == QProcess::Crashed && !m_timedOut) {
        emit grabError(QStringLiteral("JobSpy process crashed unexpectedly."));
    }
}

void JobSpyClient::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    m_timer->stop();
    if (m_timedOut) return;

    // Ensure all remaining data is flushed
    m_stdoutBuffer.append(m_process->readAllStandardOutput());
    m_stderrBuffer.append(m_process->readAllStandardError());

    const QString stdoutStr = QString::fromUtf8(m_stdoutBuffer).trimmed();
    const QString stderrStr = QString::fromUtf8(m_stderrBuffer).trimmed();

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        QString message = stderrStr;
        if (message.isEmpty()) {
            message = QStringLiteral("JobSpy bridge failed with exit code %1.").arg(exitCode);
        }
        emit grabError(message);
        return;
    }

    if (stdoutStr.isEmpty()) {
        emit grabError(QStringLiteral("JobSpy bridge returned empty output."));
        return;
    }

    QString parseErr;
    auto recordOpt = JobRecord::fromJson(stdoutStr, &parseErr);
    if (!recordOpt.has_value()) {
        emit grabError(QStringLiteral("Failed to parse scraped job data: %1").arg(parseErr));
        return;
    }

    emit grabSuccess(recordOpt.value());
}

} // namespace jobstarr
