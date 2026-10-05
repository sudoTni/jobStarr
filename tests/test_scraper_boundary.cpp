#include <QTest>
#include <QSignalSpy>
#include "scraping/JobScraper.h"
#include "scraping/ScraperFactory.h"
#include "scraping/LinkedInScraper.h"
#include "scraping/IndeedScraper.h"

using namespace jobstarr;

class TestScraperBoundary : public QObject {
    Q_OBJECT

private slots:
    void testFactoryValidation();
    void testLinkedInMalformedId();
    void testIndeedMalformedKey();
    void testLinkedInTimeout();
    void testIndeedTimeout();
    void testCancellation();
};

void TestScraperBoundary::testFactoryValidation() {
    QString err;
    auto s1 = ScraperFactory::createScraper(QStringLiteral("https://unsupported.com/job/123"), nullptr, nullptr, &err);
    QVERIFY(s1 == nullptr);
    QVERIFY(err.contains(QStringLiteral("Unsupported host")));

    auto s2 = ScraperFactory::createScraper(QStringLiteral("not-a-url"), nullptr, nullptr, &err);
    QVERIFY(s2 == nullptr);
    QVERIFY(!err.isEmpty());

    auto s3 = ScraperFactory::createScraper(QStringLiteral("https://www.linkedin.com/jobs/view/123456"), nullptr, nullptr, &err);
    QVERIFY(s3 != nullptr);

    auto s4 = ScraperFactory::createScraper(QStringLiteral("https://www.indeed.com/viewjob?jk=abcdef0123456789"), nullptr, nullptr, &err);
    QVERIFY(s4 != nullptr);
}

void TestScraperBoundary::testLinkedInMalformedId() {
    LinkedInScraper scraper;
    QSignalSpy errorSpy(&scraper, &JobScraper::error);

    scraper.scrape(QStringLiteral("https://www.linkedin.com/jobs/view/nodigits"));
    QCOMPARE(errorSpy.count(), 1);
    const QString errMsg = errorSpy.first().first().toString();
    QVERIFY(errMsg.contains(QStringLiteral("Unable to extract LinkedIn job ID")));
}

void TestScraperBoundary::testIndeedMalformedKey() {
    IndeedScraper scraper;
    QSignalSpy errorSpy(&scraper, &JobScraper::error);

    scraper.scrape(QStringLiteral("https://www.indeed.com/jobs?q=cpp"));
    QCOMPARE(errorSpy.count(), 1);
    const QString errMsg = errorSpy.first().first().toString();
    QVERIFY(errMsg.contains(QStringLiteral("Unable to extract Indeed job key")));
}

void TestScraperBoundary::testLinkedInTimeout() {
    LinkedInScraper scraper;
    QSignalSpy errorSpy(&scraper, &JobScraper::error);

    // 100ms timeout
    scraper.scrape(QStringLiteral("https://www.linkedin.com/jobs/view/9999999999"), 100);

    QVERIFY(errorSpy.wait(3000));
    QCOMPARE(errorSpy.count(), 1);
    const QString errMsg = errorSpy.first().first().toString();
    QVERIFY2(errMsg.contains(QStringLiteral("timed out")) || errMsg.contains(QStringLiteral("LinkedIn")), qPrintable(errMsg));
}

void TestScraperBoundary::testIndeedTimeout() {
    IndeedScraper scraper;
    QSignalSpy errorSpy(&scraper, &JobScraper::error);

    // 100ms timeout
    scraper.scrape(QStringLiteral("https://www.indeed.com/viewjob?jk=abcdef0123456789"), 100);

    QVERIFY(errorSpy.wait(3000));
    QCOMPARE(errorSpy.count(), 1);
    const QString errMsg = errorSpy.first().first().toString();
    QVERIFY2(errMsg.contains(QStringLiteral("timed out")) || errMsg.contains(QStringLiteral("Indeed")), qPrintable(errMsg));
}

void TestScraperBoundary::testCancellation() {
    LinkedInScraper scraper;
    QSignalSpy startedSpy(&scraper, &JobScraper::started);

    scraper.scrape(QStringLiteral("https://www.linkedin.com/jobs/view/9999999999"), 5000);
    QCOMPARE(startedSpy.count(), 1);
    QVERIFY(scraper.isRunning());

    scraper.cancel();
    QVERIFY(!scraper.isRunning());
}

QTEST_MAIN(TestScraperBoundary)
#include "test_scraper_boundary.moc"
