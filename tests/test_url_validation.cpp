#include <QTest>
#include "UrlValidator.h"

using namespace jobstarr;

class TestUrlValidation : public QObject {
    Q_OBJECT

private slots:
    void testValidLinkedInUrls();
    void testValidIndeedUrls();
    void testUnsupportedHosts();
    void testMalformedUrls();
    void testHostSpoofing();
};

void TestUrlValidation::testValidLinkedInUrls() {
    const QStringList validUrls = {
        QStringLiteral("https://www.linkedin.com/jobs/view/4464921447"),
        QStringLiteral("https://linkedin.com/jobs/view/4464921447"),
        QStringLiteral("http://www.linkedin.com/jobs/view/software-engineer-4464921447"),
        QStringLiteral("https://www.linkedin.com/jobs/view/4464921447?trackingId=xyz"),
        QStringLiteral("https://linkedin.com/jobs/collections/recommended/?currentJobId=4464921447")
    };

    for (const QString &url : validUrls) {
        QString err;
        QVERIFY2(UrlValidator::isValidJobUrl(url, &err), qPrintable(url + ": " + err));
        QCOMPARE(UrlValidator::detectSource(url), JobSource::LinkedIn);
    }
}

void TestUrlValidation::testValidIndeedUrls() {
    const QStringList validUrls = {
        QStringLiteral("https://www.indeed.com/viewjob?jk=726de2ee84058355"),
        QStringLiteral("https://indeed.com/viewjob?jk=726de2ee84058355"),
        QStringLiteral("https://ca.indeed.com/viewjob?jk=726de2ee84058355"),
        QStringLiteral("https://indeed.co.uk/viewjob?jk=726de2ee84058355"),
        QStringLiteral("http://www.indeed.com/rc/clk?jk=726de2ee84058355&from=hp"),
        QStringLiteral("https://indeed.fr/viewjob?jk=726de2ee84058355")
    };

    for (const QString &url : validUrls) {
        QString err;
        QVERIFY2(UrlValidator::isValidJobUrl(url, &err), qPrintable(url + ": " + err));
        QCOMPARE(UrlValidator::detectSource(url), JobSource::Indeed);
    }
}

void TestUrlValidation::testUnsupportedHosts() {
    const QStringList unsupportedUrls = {
        QStringLiteral("https://www.monster.com/job/123"),
        QStringLiteral("https://glassdoor.com/job/456"),
        QStringLiteral("https://google.com/jobs"),
        QStringLiteral("https://github.com/speedyapply/JobSpy")
    };

    for (const QString &url : unsupportedUrls) {
        QString err;
        QVERIFY(!UrlValidator::isValidJobUrl(url, &err));
        QVERIFY(!err.isEmpty());
        QCOMPARE(UrlValidator::detectSource(url), JobSource::Unknown);
    }
}

void TestUrlValidation::testMalformedUrls() {
    QString err;
    QVERIFY(!UrlValidator::isValidJobUrl(QString(), &err));
    QVERIFY(err.contains(QStringLiteral("empty")));

    QVERIFY(!UrlValidator::isValidJobUrl(QStringLiteral("just plain text"), &err));
    QVERIFY(!UrlValidator::isValidJobUrl(QStringLiteral("ftp://www.linkedin.com/jobs/123"), &err));
    QVERIFY(err.contains(QStringLiteral("http")));
}

void TestUrlValidation::testHostSpoofing() {
    const QStringList spoofUrls = {
        QStringLiteral("https://linkedin.com.attacker.example/jobs/view/123"),
        QStringLiteral("https://www.linkedin.com.fake.com/jobs/view/123"),
        QStringLiteral("https://indeed.com.attacker.example/viewjob?jk=123"),
        QStringLiteral("https://attackerlinkedin.com/jobs/view/123"),
        QStringLiteral("https://fakeindeed.com/viewjob?jk=123")
    };

    for (const QString &url : spoofUrls) {
        QString err;
        QVERIFY2(!UrlValidator::isValidJobUrl(url, &err), qPrintable("Should reject spoof: " + url));
        QCOMPARE(UrlValidator::detectSource(url), JobSource::Unknown);
    }
}

QTEST_MAIN(TestUrlValidation)
#include "test_url_validation.moc"
