#include <QTest>
#include <QFile>
#include <QDir>
#include "scraping/IndeedScraper.h"
#include "ProjectPaths.h"

using namespace jobstarr;

class TestIndeedParser : public QObject {
    Q_OBJECT

private slots:
    void testParseFullFixture();
    void testParseEstimatedCompensationFixture();
    void testParseEmptyResults();
    void testParseMalformedJson();
};

void TestIndeedParser::testParseFullFixture() {
    const QString fixturePath = QDir(ProjectPaths::projectRoot()).filePath(QStringLiteral("tests/fixtures/indeed_graphql_full.json"));
    QFile file(fixturePath);
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(fixturePath));
    const QByteArray data = file.readAll();
    file.close();

    QString errorMsg;
    auto recordOpt = IndeedScraper::parseJson(data, QStringLiteral("71249db25547cb07"), QStringLiteral("https://www.indeed.com"), &errorMsg);
    QVERIFY2(recordOpt.has_value(), qPrintable(errorMsg));

    const JobRecord &job = recordOpt.value();
    QCOMPARE(job.source(), QStringLiteral("indeed"));
    QCOMPARE(job.id(), QStringLiteral("in-71249db25547cb07"));
    QCOMPARE(job.title(), QStringLiteral("Principal Cyber Systems Architect"));
    QCOMPARE(job.companyName(), QStringLiteral("Cyber Defense Corp"));
    QCOMPARE(job.companyUrl(), QStringLiteral("https://www.indeed.com/cmp/cyber-defense-corp"));
    QCOMPARE(job.companyUrlDirect(), QStringLiteral("https://cyberdefense.example.com"));
    QCOMPARE(job.location(), QStringLiteral("Boston, MA, US"));
    QCOMPARE(job.isRemote(), true);

    const QStringList emails = job.emails();
    QCOMPARE(emails.size(), 1);
    QCOMPARE(emails.first(), QStringLiteral("hiring@cyberdefense.example.com"));

    const QJsonObject &obj = job.jsonObject();
    QCOMPARE(obj.value(QLatin1String("company_industry")).toString(), QStringLiteral("Information Technology"));
    QCOMPARE(obj.value(QLatin1String("company_addresses")).toString(), QStringLiteral("100 High Street, Boston, MA 02110"));
    QCOMPARE(obj.value(QLatin1String("company_logo")).toString(), QStringLiteral("https://example.com/logo.png"));

    // Compensation
    QVERIFY(obj.value(QLatin1String("compensation")).isObject());
    const QJsonObject comp = obj.value(QLatin1String("compensation")).toObject();
    QCOMPARE(comp.value(QLatin1String("interval")).toString(), QStringLiteral("yearly"));
    QCOMPARE(comp.value(QLatin1String("min_amount")).toDouble(), 175000.0);
    QCOMPARE(comp.value(QLatin1String("max_amount")).toDouble(), 220000.0);
    QCOMPARE(comp.value(QLatin1String("currency")).toString(), QStringLiteral("USD"));
}

void TestIndeedParser::testParseEstimatedCompensationFixture() {
    const QString fixturePath = QDir(ProjectPaths::projectRoot()).filePath(QStringLiteral("tests/fixtures/indeed_graphql_estimated.json"));
    QFile file(fixturePath);
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(fixturePath));
    const QByteArray data = file.readAll();
    file.close();

    QString errorMsg;
    auto recordOpt = IndeedScraper::parseJson(data, QStringLiteral("aabbccddeeff0011"), QStringLiteral("https://www.indeed.com"), &errorMsg);
    QVERIFY2(recordOpt.has_value(), qPrintable(errorMsg));

    const JobRecord &job = recordOpt.value();
    QCOMPARE(job.id(), QStringLiteral("in-aabbccddeeff0011"));
    QCOMPARE(job.title(), QStringLiteral("Junior Linux Administrator"));
    QCOMPARE(job.isRemote(), false);

    const QJsonObject &obj = job.jsonObject();
    QVERIFY(obj.value(QLatin1String("compensation")).isObject());
    const QJsonObject comp = obj.value(QLatin1String("compensation")).toObject();
    QCOMPARE(comp.value(QLatin1String("interval")).toString(), QStringLiteral("hourly"));
    QCOMPARE(comp.value(QLatin1String("min_amount")).toDouble(), 35.5);
    QCOMPARE(comp.value(QLatin1String("max_amount")).toDouble(), 48.0);
}

void TestIndeedParser::testParseEmptyResults() {
    const QString fixturePath = QDir(ProjectPaths::projectRoot()).filePath(QStringLiteral("tests/fixtures/indeed_graphql_empty.json"));
    QFile file(fixturePath);
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(fixturePath));
    const QByteArray data = file.readAll();
    file.close();

    QString errorMsg;
    auto recordOpt = IndeedScraper::parseJson(data, QStringLiteral("nonexistent123"), QStringLiteral("https://www.indeed.com"), &errorMsg);
    QVERIFY(!recordOpt.has_value());
    QVERIFY(errorMsg.contains(QStringLiteral("Indeed job not found")));
}

void TestIndeedParser::testParseMalformedJson() {
    QString errorMsg;
    auto recordOpt = IndeedScraper::parseJson(QByteArray("{ malformed"), QStringLiteral("bad"), QStringLiteral("https://www.indeed.com"), &errorMsg);
    QVERIFY(!recordOpt.has_value());
    QVERIFY(errorMsg.contains(QStringLiteral("Failed to parse")));
}

QTEST_MAIN(TestIndeedParser)
#include "test_indeed_parser.moc"
