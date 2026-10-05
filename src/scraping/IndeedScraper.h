#pragma once

#include "JobScraper.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <memory>

namespace jobstarr {

class IndeedScraper : public JobScraper {
    Q_OBJECT
public:
    explicit IndeedScraper(QNetworkAccessManager *nam = nullptr, QObject *parent = nullptr);
    ~IndeedScraper() override;

    void scrape(const QString &url, int timeoutMs = 60000) override;
    void cancel() override;
    bool isRunning() const override;

    // Public static parser method for offline unit testing
    static std::optional<JobRecord> parseJson(const QByteArray &jsonBytes,
                                             const QString &jobKey,
                                             const QString &baseUrl,
                                             QString *errorMessage = nullptr);

private slots:
    void onReplyFinished();
    void onTimeout();

private:
    QNetworkAccessManager *m_nam{nullptr};
    bool m_ownsNam{false};
    QNetworkReply *m_currentReply{nullptr};
    QTimer *m_timer{nullptr};
    QString m_currentJobKey;
    QString m_currentBaseUrl;
    bool m_timedOut{false};
};

} // namespace jobstarr
