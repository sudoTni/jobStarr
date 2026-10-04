#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include "JobJudgeResult.h"

namespace jobstarr {

class LlmClient : public QObject {
    Q_OBJECT
public:
    explicit LlmClient(QNetworkAccessManager *networkManager = nullptr, QObject *parent = nullptr);
    ~LlmClient() override;

    void sendChatCompletion(const QString &endpoint,
                            const QString &model,
                            const QString &apiKey,
                            const QString &systemPrompt,
                            const QString &userPrompt,
                            int timeoutMs = 300000,
                            const QString &reasoningEffort = QString());

    void judgeJob(const QString &endpoint,
                  const QString &model,
                  const QString &apiKey,
                  const QString &systemPrompt,
                  const QString &userPrompt,
                  int timeoutMs = 300000,
                  const QString &reasoningEffort = QString());

    void cancel();
    bool isRunning() const;

signals:
    void chatStarted();
    void chatSuccess(const QString &content);
    void chatError(const QString &errorMessage);

    void judgeStarted();
    void judgeSuccess(const JobJudgeResult &result);
    void judgeError(const QString &errorMessage);

private slots:
    void onReplyFinished();
    void onTimeout();

private:
    QNetworkAccessManager *m_networkManager{nullptr};
    bool m_ownsNetworkManager{false};
    QNetworkReply *m_currentReply{nullptr};
    QTimer *m_timer{nullptr};
    bool m_timedOut{false};
    bool m_isJudging{false};
};

} // namespace jobstarr
