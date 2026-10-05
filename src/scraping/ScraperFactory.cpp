#include "ScraperFactory.h"
#include "LinkedInScraper.h"
#include "IndeedScraper.h"
#include "UrlValidator.h"

namespace jobstarr {

std::unique_ptr<JobScraper> ScraperFactory::createScraper(const QString &url,
                                                         QNetworkAccessManager *nam,
                                                         QObject *parent,
                                                         QString *errorMessage) {
    QString valError;
    if (!UrlValidator::isValidJobUrl(url, &valError)) {
        if (errorMessage) *errorMessage = valError;
        return nullptr;
    }

    const JobSource source = UrlValidator::detectSource(url);
    if (source == JobSource::LinkedIn) {
        return std::make_unique<LinkedInScraper>(nam, parent);
    } else if (source == JobSource::Indeed) {
        return std::make_unique<IndeedScraper>(nam, parent);
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("Unsupported job board host.");
    }
    return nullptr;
}

} // namespace jobstarr
