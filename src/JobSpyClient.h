#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include "JobRecord.h"

namespace jobstarr {

class JobSpyClient : public QObject {
    Q_OBJECT
public:
    explicit JobSpyClient(QObject *parent = nullptr);
    ~JobSpyClient() override;

    void grabJob(const QString &url, int timeoutMs = 60000);
    void cancel();

    bool isRunning() const;
    static QString findBridgeScript();

signals:
    void grabStarted();
    void grabSuccess(const JobRecord &record);
    void grabError(const QString &errorMessage);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onTimeout();

private:
    QProcess *m_process{nullptr};
    QTimer *m_timer{nullptr};
    QByteArray m_stdoutBuffer;
    QByteArray m_stderrBuffer;
    bool m_timedOut{false};
};

} // namespace jobstarr
