#pragma once

#include <QObject>
#include <QString>
#include "JobRecord.h"
#include "ScrapeError.h"

namespace jobstarr {

class JobScraper : public QObject {
    Q_OBJECT
public:
    explicit JobScraper(QObject *parent = nullptr) : QObject(parent) {}
    ~JobScraper() override = default;

    virtual void scrape(const QString &url, int timeoutMs = 60000) = 0;
    virtual void cancel() = 0;
    virtual bool isRunning() const = 0;

signals:
    void started();
    void success(const JobRecord &record);
    void error(const QString &errorMessage);
};

} // namespace jobstarr
