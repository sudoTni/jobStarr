#include <QTest>
#include "CompanyAddressLookup.h"

#include <QJsonObject>

using namespace jobstarr;

class TestAddressLookup : public QObject {
    Q_OBJECT

private slots:
    void testPromptExcludesResumeAndTestimonials();
    void testValidOneLineAddress();
    void testAddressWithExtraWhitespace();
    void testMultilineAddressNormalization();
    void testUnknownResponseRejection();
    void testEmptyResponseRejection();
    void testCodeFencesStripping();
};

void TestAddressLookup::testPromptExcludesResumeAndTestimonials() {
    QJsonObject obj;
    obj[QLatin1String("id")] = QStringLiteral("test-1");
    obj[QLatin1String("title")] = QStringLiteral("Security Analyst");
    obj[QLatin1String("company_name")] = QStringLiteral("Acme Corp");
    obj[QLatin1String("location")] = QStringLiteral("New York, NY");
    obj[QLatin1String("job_url")] = QStringLiteral("https://www.linkedin.com/jobs/view/12345");
    auto jobOpt = JobRecord::fromJsonObject(obj);
    QVERIFY(jobOpt.has_value());
    const JobRecord job = jobOpt.value();

    QString prompt = CompanyAddressLookup::buildUserPrompt(job);
    QVERIFY(prompt.contains(QStringLiteral("Acme Corp")));
    QVERIFY(prompt.contains(QStringLiteral("Security Analyst")));
    QVERIFY(prompt.contains(QStringLiteral("New York, NY")));

    // Ensure candidate fields are not present
    QVERIFY(!prompt.contains(QStringLiteral("Candidate")));
    QVERIFY(!prompt.contains(QStringLiteral("Michael Martini")));
    QVERIFY(!prompt.contains(QStringLiteral("myResume")));
    QVERIFY(!prompt.contains(QStringLiteral("myTestimonials")));

    // Also test structured location object
    QJsonObject locObj;
    locObj[QLatin1String("city")] = QStringLiteral("Austin");
    locObj[QLatin1String("state")] = QStringLiteral("TX");
    locObj[QLatin1String("country")] = QStringLiteral("USA");
    obj[QLatin1String("location")] = locObj;

    auto jobStructuredOpt = JobRecord::fromJsonObject(obj);
    QVERIFY(jobStructuredOpt.has_value());
    QString promptStructured = CompanyAddressLookup::buildUserPrompt(jobStructuredOpt.value());
    QVERIFY(promptStructured.contains(QStringLiteral("Austin, TX, USA")));
}

void TestAddressLookup::testValidOneLineAddress() {
    QString raw = QStringLiteral("123 Example Avenue, Suite 400, New York, NY 10001");
    QString err;
    auto opt = CompanyAddressLookup::parseResponse(raw, &err);
    QVERIFY2(opt.has_value(), qPrintable(err));
    QCOMPARE(opt.value(), QStringLiteral("123 Example Avenue, Suite 400, New York, NY 10001"));
}

void TestAddressLookup::testAddressWithExtraWhitespace() {
    QString raw = QStringLiteral("   \n\t  123 Example Avenue, Suite 400, New York, NY 10001  \n\t ");
    auto opt = CompanyAddressLookup::parseResponse(raw);
    QVERIFY(opt.has_value());
    QCOMPARE(opt.value(), QStringLiteral("123 Example Avenue, Suite 400, New York, NY 10001"));
}

void TestAddressLookup::testMultilineAddressNormalization() {
    QString raw = QStringLiteral("123 Example Avenue\nSuite 400\nNew York, NY 10001");
    auto opt = CompanyAddressLookup::parseResponse(raw);
    QVERIFY(opt.has_value());
    QCOMPARE(opt.value(), QStringLiteral("123 Example Avenue Suite 400 New York, NY 10001"));
}

void TestAddressLookup::testUnknownResponseRejection() {
    QString err;
    auto opt1 = CompanyAddressLookup::parseResponse(QStringLiteral("UNKNOWN"), &err);
    QVERIFY(!opt1.has_value());
    QVERIFY(err.contains(QStringLiteral("UNKNOWN")));

    auto opt2 = CompanyAddressLookup::parseResponse(QStringLiteral("unknown"), &err);
    QVERIFY(!opt2.has_value());
}

void TestAddressLookup::testEmptyResponseRejection() {
    QString err;
    auto opt = CompanyAddressLookup::parseResponse(QStringLiteral("   \n  "), &err);
    QVERIFY(!opt.has_value());
}

void TestAddressLookup::testCodeFencesStripping() {
    QString raw = QStringLiteral("```text\n123 Example Avenue, Suite 400, New York, NY 10001\n```");
    auto opt = CompanyAddressLookup::parseResponse(raw);
    QVERIFY(opt.has_value());
    QCOMPARE(opt.value(), QStringLiteral("123 Example Avenue, Suite 400, New York, NY 10001"));
}

QTEST_MAIN(TestAddressLookup)
#include "test_address_lookup.moc"
