#pragma once

#include "JobScraper.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <memory>

namespace jobstarr {

class LinkedInScraper : public JobScraper {
    Q_OBJECT
public:
    explicit LinkedInScraper(QNetworkAccessManager *nam = nullptr, QObject *parent = nullptr);
    ~LinkedInScraper() override;

    void scrape(const QString &url, int timeoutMs = 60000) override;
    void cancel() override;
    bool isRunning() const override;

    // Public static parser method for offline unit testing
    static std::optional<JobRecord> parseHtml(const QString &html, const QString &jobId, QString *errorMessage = nullptr);

private slots:
    void onReplyFinished();
    void onTimeout();

private:
    QNetworkAccessManager *m_nam{nullptr};
    bool m_ownsNam{false};
    QNetworkReply *m_currentReply{nullptr};
    QTimer *m_timer{nullptr};
    QString m_currentJobId;
    bool m_timedOut{false};
};

} // namespace jobstarr
