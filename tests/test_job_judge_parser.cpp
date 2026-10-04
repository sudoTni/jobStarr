#include <QTest>
#include "JobJudgeResult.h"

using namespace jobstarr;

class TestJobJudgeParser : public QObject {
    Q_OBJECT

private slots:
    void testValidTrueResponse();
    void testValidFalseResponse();
    void testJsonInsideCodeFence();
    void testMissingField();
    void testWrongFieldType();
    void testEmptyArray();
    void testMultipleObjects();
    void testConfidenceRange();
    void testNonJsonProse();
    void testMalformedJson();
};

void TestJobJudgeParser::testValidTrueResponse() {
    const QString json = QStringLiteral(R"([
  {
    "jobTitle": "Senior Security Engineer",
    "isVeryHighlyAligned": true,
    "rationale": "Strong alignment across EDR and incident response. Recommend applying.",
    "confidence": 0.95
  }
])");

    QString err;
    auto res = JobJudgeResult::parseLlmResponse(json, &err);
    QVERIFY2(res.has_value(), qPrintable(err));
    QCOMPARE(res->jobTitle, QStringLiteral("Senior Security Engineer"));
    QCOMPARE(res->isVeryHighlyAligned, true);
    QCOMPARE(res->rationale, QStringLiteral("Strong alignment across EDR and incident response. Recommend applying."));
    QCOMPARE(res->confidence, 0.95);
}

void TestJobJudgeParser::testValidFalseResponse() {
    const QString json = QStringLiteral(R"([
  {
    "jobTitle": "DevOps Architect",
    "isVeryHighlyAligned": false,
    "rationale": "Role requires 10 years of Kubernetes administration. Do not recommend applying.",
    "confidence": 0.88
  }
])");

    QString err;
    auto res = JobJudgeResult::parseLlmResponse(json, &err);
    QVERIFY2(res.has_value(), qPrintable(err));
    QCOMPARE(res->jobTitle, QStringLiteral("DevOps Architect"));
    QCOMPARE(res->isVeryHighlyAligned, false);
    QCOMPARE(res->confidence, 0.88);
}

void TestJobJudgeParser::testJsonInsideCodeFence() {
    const QString withFence = QStringLiteral(
        "```json\n"
        "[\n"
        "  {\n"
        "    \"jobTitle\": \"Support Engineer\",\n"
        "    \"isVeryHighlyAligned\": true,\n"
        "    \"rationale\": \"Great match. Recommend applying.\",\n"
        "    \"confidence\": 0.90\n"
        "  }\n"
        "]\n"
        "```"
    );

    QString err;
    auto res = JobJudgeResult::parseLlmResponse(withFence, &err);
    QVERIFY2(res.has_value(), qPrintable(err));
    QCOMPARE(res->jobTitle, QStringLiteral("Support Engineer"));
    QCOMPARE(res->isVeryHighlyAligned, true);

    // Also test fence without "json" identifier
    const QString plainFence = QStringLiteral(
        "```\n"
        "[\n"
        "  {\n"
        "    \"jobTitle\": \"Support Engineer\",\n"
        "    \"isVeryHighlyAligned\": true,\n"
        "    \"rationale\": \"Great match. Recommend applying.\",\n"
        "    \"confidence\": 0.90\n"
        "  }\n"
        "]\n"
        "```"
    );
    res = JobJudgeResult::parseLlmResponse(plainFence, &err);
    QVERIFY2(res.has_value(), qPrintable(err));
}

void TestJobJudgeParser::testMissingField() {
    QString err;

    // Missing jobTitle
    const QString missingTitle = QStringLiteral(R"([{"isVeryHighlyAligned":true,"rationale":"ok","confidence":0.9}])");
    QVERIFY(!JobJudgeResult::parseLlmResponse(missingTitle, &err).has_value());
    QVERIFY(err.contains(QStringLiteral("jobTitle")));

    // Missing isVeryHighlyAligned
    const QString missingAligned = QStringLiteral(R"([{"jobTitle":"SE","rationale":"ok","confidence":0.9}])");
    QVERIFY(!JobJudgeResult::parseLlmResponse(missingAligned, &err).has_value());
    QVERIFY(err.contains(QStringLiteral("isVeryHighlyAligned")));

    // Missing rationale
    const QString missingRationale = QStringLiteral(R"([{"jobTitle":"SE","isVeryHighlyAligned":true,"confidence":0.9}])");
    QVERIFY(!JobJudgeResult::parseLlmResponse(missingRationale, &err).has_value());
    QVERIFY(err.contains(QStringLiteral("rationale")));

    // Missing confidence
    const QString missingConf = QStringLiteral(R"([{"jobTitle":"SE","isVeryHighlyAligned":true,"rationale":"ok"}])");
    QVERIFY(!JobJudgeResult::parseLlmResponse(missingConf, &err).has_value());
    QVERIFY(err.contains(QStringLiteral("confidence")));
}

void TestJobJudgeParser::testWrongFieldType() {
    QString err;

    // isVeryHighlyAligned as string instead of boolean
    const QString badBool = QStringLiteral(R"([{"jobTitle":"SE","isVeryHighlyAligned":"true","rationale":"ok","confidence":0.9}])");
    QVERIFY(!JobJudgeResult::parseLlmResponse(badBool, &err).has_value());

    // confidence as string instead of number
    const QString badConf = QStringLiteral(R"([{"jobTitle":"SE","isVeryHighlyAligned":true,"rationale":"ok","confidence":"0.9"}])");
    QVERIFY(!JobJudgeResult::parseLlmResponse(badConf, &err).has_value());

    // jobTitle as number
    const QString badTitle = QStringLiteral(R"([{"jobTitle":123,"isVeryHighlyAligned":true,"rationale":"ok","confidence":0.9}])");
    QVERIFY(!JobJudgeResult::parseLlmResponse(badTitle, &err).has_value());
}

void TestJobJudgeParser::testEmptyArray() {
    QString err;
    QVERIFY(!JobJudgeResult::parseLlmResponse(QStringLiteral("[]"), &err).has_value());
    QVERIFY(err.contains(QStringLiteral("exactly 1 object")));
}

void TestJobJudgeParser::testMultipleObjects() {
    const QString twoObjs = QStringLiteral(R"([
  {"jobTitle":"SE1","isVeryHighlyAligned":true,"rationale":"ok","confidence":0.9},
  {"jobTitle":"SE2","isVeryHighlyAligned":false,"rationale":"no","confidence":0.8}
])");
    QString err;
    QVERIFY(!JobJudgeResult::parseLlmResponse(twoObjs, &err).has_value());
    QVERIFY(err.contains(QStringLiteral("found 2")));
}

void TestJobJudgeParser::testConfidenceRange() {
    QString err;

    // Confidence < 0.0
    const QString negativeConf = QStringLiteral(R"([{"jobTitle":"SE","isVeryHighlyAligned":true,"rationale":"ok","confidence":-0.1}])");
    QVERIFY(!JobJudgeResult::parseLlmResponse(negativeConf, &err).has_value());
    QVERIFY(err.contains(QStringLiteral("between 0.0 and 1.0")));

    // Confidence > 1.0
    const QString overConf = QStringLiteral(R"([{"jobTitle":"SE","isVeryHighlyAligned":true,"rationale":"ok","confidence":1.05}])");
    QVERIFY(!JobJudgeResult::parseLlmResponse(overConf, &err).has_value());
    QVERIFY(err.contains(QStringLiteral("between 0.0 and 1.0")));

    // Boundary 0.0 and 1.0 are valid
    const QString zeroConf = QStringLiteral(R"([{"jobTitle":"SE","isVeryHighlyAligned":false,"rationale":"ok","confidence":0.0}])");
    QVERIFY(JobJudgeResult::parseLlmResponse(zeroConf, &err).has_value());

    const QString oneConf = QStringLiteral(R"([{"jobTitle":"SE","isVeryHighlyAligned":true,"rationale":"ok","confidence":1.0}])");
    QVERIFY(JobJudgeResult::parseLlmResponse(oneConf, &err).has_value());
}

void TestJobJudgeParser::testNonJsonProse() {
    const QString prose = QStringLiteral("Here is my evaluation: The candidate is a great match!");
    QString err;
    QVERIFY(!JobJudgeResult::parseLlmResponse(prose, &err).has_value());
    QVERIFY(!err.isEmpty());
}

void TestJobJudgeParser::testMalformedJson() {
    const QString malformed = QStringLiteral("[ { \"jobTitle\": \"SE\", unquoted: broken } ]");
    QString err;
    QVERIFY(!JobJudgeResult::parseLlmResponse(malformed, &err).has_value());
    QVERIFY(err.contains(QStringLiteral("Invalid JSON")));
}

QTEST_MAIN(TestJobJudgeParser)
#include "test_job_judge_parser.moc"
