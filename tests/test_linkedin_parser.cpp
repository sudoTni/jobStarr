#include <QTest>
#include <QFile>
#include <QDir>
#include "scraping/LinkedInScraper.h"
#include "ProjectPaths.h"

using namespace jobstarr;

class TestLinkedInParser : public QObject {
    Q_OBJECT

private slots:
    void testParseFullFixture();
    void testParseMinimalFixture();
    void testParseEmptyHtml();
};

void TestLinkedInParser::testParseFullFixture() {
    const QString fixturePath = QDir(ProjectPaths::projectRoot()).filePath(QStringLiteral("tests/fixtures/linkedin_full.html"));
    QFile file(fixturePath);
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(fixturePath));
    const QString html = QString::fromUtf8(file.readAll());
    file.close();

    QString errorMsg;
    auto recordOpt = LinkedInScraper::parseHtml(html, QStringLiteral("4464921447"), &errorMsg);
    QVERIFY2(recordOpt.has_value(), qPrintable(errorMsg));

    const JobRecord &job = recordOpt.value();
    QCOMPARE(job.source(), QStringLiteral("linkedin"));
    QCOMPARE(job.id(), QStringLiteral("li-4464921447"));
    QCOMPARE(job.title(), QStringLiteral("Staff Security Engineer"));
    QCOMPARE(job.companyName(), QStringLiteral("Acme Systems"));
    QCOMPARE(job.companyUrl(), QStringLiteral("https://www.linkedin.com/company/acme-systems"));
    QCOMPARE(job.location(), QStringLiteral("New York, NY, USA"));
    QCOMPARE(job.datePosted(), QStringLiteral("2026-10-02"));
    QCOMPARE(job.jobUrl(), QStringLiteral("https://www.linkedin.com/jobs/view/4464921447"));
    QCOMPARE(job.isRemote(), false);

    // Emails
    const QStringList emails = job.emails();
    QCOMPARE(emails.size(), 2);
    QVERIFY(emails.contains(QStringLiteral("recruiting@acme-systems.com")));
    QVERIFY(emails.contains(QStringLiteral("sec.ops@acme-systems.com")));

    // JSON fields
    const QJsonObject &obj = job.jsonObject();
    QCOMPARE(obj.value(QLatin1String("job_level")).toString(), QStringLiteral("mid-senior level"));
    QCOMPARE(obj.value(QLatin1String("job_function")).toString(), QStringLiteral("Information Technology and Engineering"));
    QCOMPARE(obj.value(QLatin1String("company_industry")).toString(), QStringLiteral("Computer and Network Security"));
    QCOMPARE(obj.value(QLatin1String("company_logo")).toString(), QStringLiteral("https://media.licdn.com/dms/image/acme_logo.png"));

    // Compensation
    QVERIFY(obj.value(QLatin1String("compensation")).isObject());
    const QJsonObject comp = obj.value(QLatin1String("compensation")).toObject();
    QCOMPARE(comp.value(QLatin1String("interval")).toString(), QStringLiteral("yearly"));
    QCOMPARE(comp.value(QLatin1String("min_amount")).toDouble(), 185000.0);
    QCOMPARE(comp.value(QLatin1String("max_amount")).toDouble(), 245000.0);
    QCOMPARE(comp.value(QLatin1String("currency")).toString(), QStringLiteral("USD"));
}

void TestLinkedInParser::testParseMinimalFixture() {
    const QString fixturePath = QDir(ProjectPaths::projectRoot()).filePath(QStringLiteral("tests/fixtures/linkedin_minimal.html"));
    QFile file(fixturePath);
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(fixturePath));
    const QString html = QString::fromUtf8(file.readAll());
    file.close();

    QString errorMsg;
    auto recordOpt = LinkedInScraper::parseHtml(html, QStringLiteral("123456"), &errorMsg);
    QVERIFY2(recordOpt.has_value(), qPrintable(errorMsg));

    const JobRecord &job = recordOpt.value();
    QCOMPARE(job.source(), QStringLiteral("linkedin"));
    QCOMPARE(job.id(), QStringLiteral("li-123456"));
    QCOMPARE(job.title(), QStringLiteral("C++ Software Intern (Remote)"));
    QCOMPARE(job.companyName(), QStringLiteral("Startup Labs"));
    QCOMPARE(job.location(), QStringLiteral("Austin, TX, USA"));
    QCOMPARE(job.isRemote(), true);
}

void TestLinkedInParser::testParseEmptyHtml() {
    QString errorMsg;
    auto recordOpt = LinkedInScraper::parseHtml(QString(), QStringLiteral("999"), &errorMsg);
    QVERIFY(!recordOpt.has_value());
    QVERIFY(!errorMsg.isEmpty());
}

QTEST_MAIN(TestLinkedInParser)
#include "test_linkedin_parser.moc"
