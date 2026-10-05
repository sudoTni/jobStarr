#include <QTest>
#include "scraping/JobParsingUtilities.h"

using namespace jobstarr;

class TestJobParsingUtilities : public QObject {
    Q_OBJECT

private slots:
    void testExtractEmails();
    void testHtmlToMarkdown();
    void testParseRelativeDate();
    void testIsJobRemote();
    void testIsIndeedJobRemote();
    void testExtractLinkedInJobId();
    void testExtractIndeedJobKey();
    void testIndeedCountryAndSubdomain();
    void testParseLinkedInCompensation();
    void testParseIndeedCompensation();
    void testJobTypeMapping();
};

void TestJobParsingUtilities::testExtractEmails() {
    const QString text = QStringLiteral(
        "For questions contact careers@example.com or hr.team@sub.domain.org! "
        "Also careers@example.com is duplicated. Invalid: hello@world."
    );
    const QStringList emails = JobParsingUtilities::extractEmails(text);
    QCOMPARE(emails.size(), 2);
    QCOMPARE(emails.at(0), QStringLiteral("careers@example.com"));
    QCOMPARE(emails.at(1), QStringLiteral("hr.team@sub.domain.org"));

    QVERIFY(JobParsingUtilities::extractEmails(QString()).isEmpty());
}

void TestJobParsingUtilities::testHtmlToMarkdown() {
    const QString html = QStringLiteral(
        "<h1>Lead Security Engineer</h1>"
        "<p>We are hiring! <strong>Must have</strong> C++ experience.</p>"
        "<ul><li>Requirement 1</li><li>Requirement 2</li></ul>"
        "<p>Apply at <a href=\"https://example.com\">careers site</a>.</p>"
    );
    const QString md = JobParsingUtilities::htmlToMarkdown(html);
    QVERIFY(md.contains(QStringLiteral("# Lead Security Engineer")));
    QVERIFY(md.contains(QStringLiteral("**Must have**")));
    QVERIFY(md.contains(QStringLiteral("- Requirement 1")));
    QVERIFY(md.contains(QStringLiteral("[careers site](https://example.com)")));
}

void TestJobParsingUtilities::testParseRelativeDate() {
    const QDate baseDate(2026, 10, 5);

    QCOMPARE(JobParsingUtilities::parseRelativeDate(QStringLiteral("3 days ago"), baseDate), QStringLiteral("2026-10-02"));
    QCOMPARE(JobParsingUtilities::parseRelativeDate(QStringLiteral("1 week ago"), baseDate), QStringLiteral("2026-09-28"));
    QCOMPARE(JobParsingUtilities::parseRelativeDate(QStringLiteral("2 weeks ago"), baseDate), QStringLiteral("2026-09-21"));
    QCOMPARE(JobParsingUtilities::parseRelativeDate(QStringLiteral("1 month ago"), baseDate), QStringLiteral("2026-09-05"));
    QCOMPARE(JobParsingUtilities::parseRelativeDate(QStringLiteral("4 hours ago"), baseDate), QStringLiteral("2026-10-05"));
    QCOMPARE(JobParsingUtilities::parseRelativeDate(QStringLiteral("Just posted"), baseDate), QString());
}

void TestJobParsingUtilities::testIsJobRemote() {
    QVERIFY(JobParsingUtilities::isJobRemote(QStringLiteral("Senior C++ Developer - Remote"), QStringLiteral("Austin, TX")));
    QVERIFY(JobParsingUtilities::isJobRemote(QStringLiteral("Security Analyst"), QStringLiteral("Remote, United States")));
    QVERIFY(JobParsingUtilities::isJobRemote(QStringLiteral("DevOps (Work from Home)"), QStringLiteral("New York, NY")));
    QVERIFY(JobParsingUtilities::isJobRemote(QStringLiteral("Architect (WFH)"), QStringLiteral("Chicago, IL")));
    QVERIFY(!JobParsingUtilities::isJobRemote(QStringLiteral("On-site Security Guard"), QStringLiteral("Boston, MA")));
}

void TestJobParsingUtilities::testIsIndeedJobRemote() {
    QJsonArray attributes;
    QJsonObject attr1;
    attr1[QStringLiteral("key")] = QStringLiteral("DSQF7");
    attr1[QStringLiteral("label")] = QStringLiteral("Remote");
    attributes.append(attr1);

    QVERIFY(JobParsingUtilities::isIndeedJobRemote(attributes, QStringLiteral("Austin, TX")));

    QJsonArray emptyAttrs;
    QVERIFY(JobParsingUtilities::isIndeedJobRemote(emptyAttrs, QStringLiteral("Remote in United States")));
    QVERIFY(!JobParsingUtilities::isIndeedJobRemote(emptyAttrs, QStringLiteral("Albany, NY")));
}

void TestJobParsingUtilities::testExtractLinkedInJobId() {
    QCOMPARE(JobParsingUtilities::extractLinkedInJobId(
        QStringLiteral("https://www.linkedin.com/jobs/view/4464921447")),
        QStringLiteral("4464921447")
    );
    QCOMPARE(JobParsingUtilities::extractLinkedInJobId(
        QStringLiteral("https://www.linkedin.com/jobs/view/senior-security-engineer-at-acme-4464921447?refId=123")),
        QStringLiteral("4464921447")
    );
    QCOMPARE(JobParsingUtilities::extractLinkedInJobId(
        QStringLiteral("https://www.linkedin.com/jobs/search/?currentJobId=9988776655&keywords=cpp")),
        QStringLiteral("9988776655")
    );
    QCOMPARE(JobParsingUtilities::extractLinkedInJobId(QStringLiteral("https://invalid.com/test")), QString());
}

void TestJobParsingUtilities::testExtractIndeedJobKey() {
    QCOMPARE(JobParsingUtilities::extractIndeedJobKey(
        QStringLiteral("https://www.indeed.com/viewjob?jk=71249db25547cb07")),
        QStringLiteral("71249db25547cb07")
    );
    QCOMPARE(JobParsingUtilities::extractIndeedJobKey(
        QStringLiteral("https://www.indeed.com/rc/clk?jk=1234567890abcdef&from=vj")),
        QStringLiteral("1234567890abcdef")
    );
    QCOMPARE(JobParsingUtilities::extractIndeedJobKey(
        QStringLiteral("https://www.indeed.com/jobs?q=cpp&vjk=fedcba0987654321")),
        QStringLiteral("fedcba0987654321")
    );
    QCOMPARE(JobParsingUtilities::extractIndeedJobKey(QStringLiteral("https://indeed.com/invalid")), QString());
}

void TestJobParsingUtilities::testIndeedCountryAndSubdomain() {
    auto resUS = JobParsingUtilities::indeedCountryAndSubdomain(QStringLiteral("www.indeed.com"));
    QCOMPARE(resUS.first, QStringLiteral("www"));
    QCOMPARE(resUS.second, QStringLiteral("US"));

    auto resUK = JobParsingUtilities::indeedCountryAndSubdomain(QStringLiteral("uk.indeed.com"));
    QCOMPARE(resUK.first, QStringLiteral("uk"));
    QCOMPARE(resUK.second, QStringLiteral("GB"));

    auto resFR = JobParsingUtilities::indeedCountryAndSubdomain(QStringLiteral("fr.indeed.com"));
    QCOMPARE(resFR.first, QStringLiteral("fr"));
    QCOMPARE(resFR.second, QStringLiteral("FR"));
}

void TestJobParsingUtilities::testParseLinkedInCompensation() {
    const auto res = JobParsingUtilities::parseLinkedInCompensation(QStringLiteral("$117,000.00/yr - $234,000.00/yr"));
    QVERIFY(res.has_value());
    QCOMPARE(res->value(QStringLiteral("interval")).toString(), QStringLiteral("yearly"));
    QCOMPARE(res->value(QStringLiteral("min_amount")).toDouble(), 117000.0);
    QCOMPARE(res->value(QStringLiteral("max_amount")).toDouble(), 234000.0);
    QCOMPARE(res->value(QStringLiteral("currency")).toString(), QStringLiteral("USD"));

    const auto resHourly = JobParsingUtilities::parseLinkedInCompensation(QStringLiteral("£45.00/hr - £60.00/hr"));
    QVERIFY(resHourly.has_value());
    QCOMPARE(resHourly->value(QStringLiteral("interval")).toString(), QStringLiteral("hourly"));
    QCOMPARE(resHourly->value(QStringLiteral("min_amount")).toDouble(), 45.0);
    QCOMPARE(resHourly->value(QStringLiteral("max_amount")).toDouble(), 60.0);
    QCOMPARE(resHourly->value(QStringLiteral("currency")).toString(), QStringLiteral("GBP"));
}

void TestJobParsingUtilities::testParseIndeedCompensation() {
    QJsonObject baseSalary;
    baseSalary[QStringLiteral("unitOfWork")] = QStringLiteral("YEAR");
    QJsonObject range;
    range[QStringLiteral("min")] = 145000.50;
    range[QStringLiteral("max")] = 195000.00;
    baseSalary[QStringLiteral("range")] = range;

    QJsonObject compObj;
    compObj[QStringLiteral("baseSalary")] = baseSalary;
    compObj[QStringLiteral("currencyCode")] = QStringLiteral("USD");

    const auto res = JobParsingUtilities::parseIndeedCompensation(compObj);
    QVERIFY(res.has_value());
    QCOMPARE(res->value(QStringLiteral("interval")).toString(), QStringLiteral("yearly"));
    QCOMPARE(res->value(QStringLiteral("min_amount")).toDouble(), 145000.50);
    QCOMPARE(res->value(QStringLiteral("max_amount")).toDouble(), 195000.00);
    QCOMPARE(res->value(QStringLiteral("currency")).toString(), QStringLiteral("USD"));
}

void TestJobParsingUtilities::testJobTypeMapping() {
    QCOMPARE(JobParsingUtilities::mapJobTypeString(QStringLiteral("Full-time")), QStringLiteral("fulltime"));
    QCOMPARE(JobParsingUtilities::mapJobTypeString(QStringLiteral("Part-time")), QStringLiteral("parttime"));
    QCOMPARE(JobParsingUtilities::mapJobTypeString(QStringLiteral("Contract")), QStringLiteral("contract"));

    QJsonArray attrs;
    QJsonObject obj1;
    obj1[QStringLiteral("key")] = QStringLiteral("CF3CP");
    attrs.append(obj1);
    const QStringList types = JobParsingUtilities::mapIndeedJobTypeAttributes(attrs);
    QCOMPARE(types.size(), 1);
    QCOMPARE(types.first(), QStringLiteral("fulltime"));
}

QTEST_MAIN(TestJobParsingUtilities)
#include "test_job_parsing_utilities.moc"
