#pragma once

#include "JobScraper.h"
#include <QNetworkAccessManager>
#include <memory>

namespace jobstarr {

class ScraperFactory {
public:
    static std::unique_ptr<JobScraper> createScraper(const QString &url,
                                                     QNetworkAccessManager *nam = nullptr,
                                                     QObject *parent = nullptr,
                                                     QString *errorMessage = nullptr);
};

} // namespace jobstarr
