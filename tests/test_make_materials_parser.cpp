#include <QTest>
#include "MakeMaterialsParser.h"

using namespace jobstarr;

class TestMakeMaterialsParser : public QObject {
    Q_OBJECT

private slots:
    void testValidCompleteResponse();
    void testCrlfResponse();
    void testLeadingAndTrailingWhitespace();
    void testMultilineProfessionalSummary();
    void testMultilineCoverLetter();
    void testFourParagraphCoverLetter();
    void testMissingResumeFilename();
    void testMissingCoverLetterFilename();
    void testMissingTitle();
    void testMissingSummary();
    void testMissingSkills();
    void testMissingCoverBody();
    void testDuplicatedRecognizedHeading();
    void testHeadingsOutOfExpectedOrder();
    void testEmptySection();
    void testUnknownExtraHeadingOrContent();
    void testCoverLetterOverFourParagraphs();
};

static QString createValidSample() {
    return QStringLiteral(
        "# Resume Filename\n"
        "Candidate_Resume_Acme\n\n"
        "# Cover Letter Filename\n"
        "Candidate_Cover_Acme\n\n"
        "# Optimized & Tailored Professional Title\n"
        "Senior Endpoint Security Engineer\n\n"
        "# Optimized & Tailored Professional Summary\n"
        "Dedicated engineer with 12+ years of enterprise IT and SecOps experience.\n\n"
        "# Optimized & Tailored Key Skills\n"
        "Splunk, Carbon Black, Active Directory, Incident Response\n\n"
        "# Optimized & Tailored Cover Letter\n"
        "I am writing to express my strong enthusiasm for the role at your organization.\n\n"
        "Throughout my career, I have protected enterprise environments and streamlined operations.\n\n"
        "My background aligns closely with your stated requirements for security and support.\n\n"
        "Thank you for your time and consideration of my candidacy."
    );
}

void TestMakeMaterialsParser::testValidCompleteResponse() {
    QString err;
    auto resOpt = MakeMaterialsParser::parse(createValidSample(), &err);
    QVERIFY2(resOpt.has_value(), qPrintable(err));

    const MakeMaterialsResult &res = resOpt.value();
    QCOMPARE(res.resumeFilename, QStringLiteral("Candidate_Resume_Acme"));
    QCOMPARE(res.coverLetterFilename, QStringLiteral("Candidate_Cover_Acme"));
    QCOMPARE(res.professionalTitle, QStringLiteral("Senior Endpoint Security Engineer"));
    QVERIFY(res.professionalSummary.contains(QStringLiteral("Dedicated engineer")));
    QCOMPARE(res.optimizedSkills, QStringLiteral("Splunk, Carbon Black, Active Directory, Incident Response"));
    QCOMPARE(res.coverLetterParagraphCount, 4);
    QVERIFY(res.coverLetterWordCount > 0);
}

void TestMakeMaterialsParser::testCrlfResponse() {
    QString sample = createValidSample();
    sample.replace(QLatin1Char('\n'), QStringLiteral("\r\n"));

    QString err;
    auto resOpt = MakeMaterialsParser::parse(sample, &err);
    QVERIFY2(resOpt.has_value(), qPrintable(err));
    QCOMPARE(resOpt->resumeFilename, QStringLiteral("Candidate_Resume_Acme"));
    QCOMPARE(resOpt->coverLetterParagraphCount, 4);
}

void TestMakeMaterialsParser::testLeadingAndTrailingWhitespace() {
    QString sample = QStringLiteral("\n\n\n  \t  \n") + createValidSample() + QStringLiteral("\n\n   \t\n");
    QString err;
    auto resOpt = MakeMaterialsParser::parse(sample, &err);
    QVERIFY2(resOpt.has_value(), qPrintable(err));
    QCOMPARE(resOpt->resumeFilename, QStringLiteral("Candidate_Resume_Acme"));
}

void TestMakeMaterialsParser::testMultilineProfessionalSummary() {
    QString multilineSummary = QStringLiteral(
        "Line 1: High level summary.\n"
        "Line 2: Regulated healthcare experience.\n"
        "Line 3: Key security initiatives."
    );
    QString sample = QStringLiteral(
        "# Resume Filename\n"
        "Res_Name\n\n"
        "# Cover Letter Filename\n"
        "Cov_Name\n\n"
        "# Optimized & Tailored Professional Title\n"
        "Title\n\n"
        "# Optimized & Tailored Professional Summary\n"
        "%1\n\n"
        "# Optimized & Tailored Key Skills\n"
        "Skills\n\n"
        "# Optimized & Tailored Cover Letter\n"
        "Cover letter paragraph 1.\n\nParagraph 2."
    ).arg(multilineSummary);

    QString err;
    auto resOpt = MakeMaterialsParser::parse(sample, &err);
    QVERIFY2(resOpt.has_value(), qPrintable(err));
    QCOMPARE(resOpt->professionalSummary, multilineSummary);
}

void TestMakeMaterialsParser::testMultilineCoverLetter() {
    QString sample = QStringLiteral(
        "# Resume Filename\n"
        "Res\n\n"
        "# Cover Letter Filename\n"
        "Cov\n\n"
        "# Optimized & Tailored Professional Title\n"
        "Title\n\n"
        "# Optimized & Tailored Professional Summary\n"
        "Summary\n\n"
        "# Optimized & Tailored Key Skills\n"
        "Skills\n\n"
        "# Optimized & Tailored Cover Letter\n"
        "First line of para 1.\nSecond line of para 1.\n\n"
        "Para 2 line."
    );

    QString err;
    auto resOpt = MakeMaterialsParser::parse(sample, &err);
    QVERIFY2(resOpt.has_value(), qPrintable(err));
    QCOMPARE(resOpt->coverLetterParagraphCount, 2);
    QVERIFY(resOpt->coverLetterBody.contains(QStringLiteral("Second line of para 1.")));
}

void TestMakeMaterialsParser::testFourParagraphCoverLetter() {
    QString sample = createValidSample();
    QString err;
    auto resOpt = MakeMaterialsParser::parse(sample, &err);
    QVERIFY2(resOpt.has_value(), qPrintable(err));
    QCOMPARE(resOpt->coverLetterParagraphCount, 4);
}

void TestMakeMaterialsParser::testMissingResumeFilename() {
    QString sample = QStringLiteral(
        "# Cover Letter Filename\n"
        "Cov\n\n"
        "# Optimized & Tailored Professional Title\n"
        "Title\n\n"
        "# Optimized & Tailored Professional Summary\n"
        "Summary\n\n"
        "# Optimized & Tailored Key Skills\n"
        "Skills\n\n"
        "# Optimized & Tailored Cover Letter\n"
        "Cover"
    );

    QString err;
    auto resOpt = MakeMaterialsParser::parse(sample, &err);
    QVERIFY(!resOpt.has_value());
    QVERIFY(err.contains(QStringLiteral("Resume Filename")));
}

void TestMakeMaterialsParser::testMissingCoverLetterFilename() {
    QString sample = QStringLiteral(
        "# Resume Filename\n"
        "Res\n\n"
        "# Optimized & Tailored Professional Title\n"
        "Title\n\n"
        "# Optimized & Tailored Professional Summary\n"
        "Summary\n\n"
        "# Optimized & Tailored Key Skills\n"
        "Skills\n\n"
        "# Optimized & Tailored Cover Letter\n"
        "Cover"
    );

    QString err;
    auto resOpt = MakeMaterialsParser::parse(sample, &err);
    QVERIFY(!resOpt.has_value());
    QVERIFY(err.contains(QStringLiteral("Cover Letter Filename")));
}

void TestMakeMaterialsParser::testMissingTitle() {
    QString sample = QStringLiteral(
        "# Resume Filename\n"
        "Res\n\n"
        "# Cover Letter Filename\n"
        "Cov\n\n"
        "# Optimized & Tailored Professional Summary\n"
        "Summary\n\n"
        "# Optimized & Tailored Key Skills\n"
        "Skills\n\n"
        "# Optimized & Tailored Cover Letter\n"
        "Cover"
    );

    QString err;
    auto resOpt = MakeMaterialsParser::parse(sample, &err);
    QVERIFY(!resOpt.has_value());
    QVERIFY(err.contains(QStringLiteral("Professional Title")));
}

void TestMakeMaterialsParser::testMissingSummary() {
    QString sample = QStringLiteral(
        "# Resume Filename\n"
        "Res\n\n"
        "# Cover Letter Filename\n"
        "Cov\n\n"
        "# Optimized & Tailored Professional Title\n"
        "Title\n\n"
        "# Optimized & Tailored Key Skills\n"
        "Skills\n\n"
        "# Optimized & Tailored Cover Letter\n"
        "Cover"
    );

    QString err;
    auto resOpt = MakeMaterialsParser::parse(sample, &err);
    QVERIFY(!resOpt.has_value());
    QVERIFY(err.contains(QStringLiteral("Professional Summary")));
}

void TestMakeMaterialsParser::testMissingSkills() {
    QString sample = QStringLiteral(
        "# Resume Filename\n"
        "Res\n\n"
        "# Cover Letter Filename\n"
        "Cov\n\n"
        "# Optimized & Tailored Professional Title\n"
        "Title\n\n"
        "# Optimized & Tailored Professional Summary\n"
        "Summary\n\n"
        "# Optimized & Tailored Cover Letter\n"
        "Cover"
    );

    QString err;
    auto resOpt = MakeMaterialsParser::parse(sample, &err);
    QVERIFY(!resOpt.has_value());
    QVERIFY(err.contains(QStringLiteral("Key Skills")));
}

void TestMakeMaterialsParser::testMissingCoverBody() {
    QString sample = QStringLiteral(
        "# Resume Filename\n"
        "Res\n\n"
        "# Cover Letter Filename\n"
        "Cov\n\n"
        "# Optimized & Tailored Professional Title\n"
        "Title\n\n"
        "# Optimized & Tailored Professional Summary\n"
        "Summary\n\n"
        "# Optimized & Tailored Key Skills\n"
        "Skills\n"
    );

    QString err;
    auto resOpt = MakeMaterialsParser::parse(sample, &err);
    QVERIFY(!resOpt.has_value());
    QVERIFY(err.contains(QStringLiteral("Cover Letter")));
}

void TestMakeMaterialsParser::testDuplicatedRecognizedHeading() {
    QString sample = QStringLiteral(
        "# Resume Filename\n"
        "Res1\n\n"
        "# Resume Filename\n"
        "Res2\n\n"
        "# Cover Letter Filename\n"
        "Cov\n\n"
        "# Optimized & Tailored Professional Title\n"
        "Title\n\n"
        "# Optimized & Tailored Professional Summary\n"
        "Summary\n\n"
        "# Optimized & Tailored Key Skills\n"
        "Skills\n\n"
        "# Optimized & Tailored Cover Letter\n"
        "Cover"
    );

    QString err;
    auto resOpt = MakeMaterialsParser::parse(sample, &err);
    QVERIFY(!resOpt.has_value());
    QVERIFY(err.contains(QStringLiteral("Duplicated recognized heading")));
}

void TestMakeMaterialsParser::testHeadingsOutOfExpectedOrder() {
    QString sample = QStringLiteral(
        "# Cover Letter Filename\n"
        "Cov\n\n"
        "# Resume Filename\n"
        "Res\n\n"
        "# Optimized & Tailored Professional Title\n"
        "Title\n\n"
        "# Optimized & Tailored Professional Summary\n"
        "Summary\n\n"
        "# Optimized & Tailored Key Skills\n"
        "Skills\n\n"
        "# Optimized & Tailored Cover Letter\n"
        "Cover"
    );

    QString err;
    auto resOpt = MakeMaterialsParser::parse(sample, &err);
    QVERIFY(!resOpt.has_value());
    QVERIFY(err.contains(QStringLiteral("Headings out of expected order")));
}

void TestMakeMaterialsParser::testEmptySection() {
    QString sample = QStringLiteral(
        "# Resume Filename\n\n"
        "# Cover Letter Filename\n"
        "Cov\n\n"
        "# Optimized & Tailored Professional Title\n"
        "Title\n\n"
        "# Optimized & Tailored Professional Summary\n"
        "Summary\n\n"
        "# Optimized & Tailored Key Skills\n"
        "Skills\n\n"
        "# Optimized & Tailored Cover Letter\n"
        "Cover"
    );

    QString err;
    auto resOpt = MakeMaterialsParser::parse(sample, &err);
    QVERIFY(!resOpt.has_value());
    QVERIFY(err.contains(QStringLiteral("empty")));
}

void TestMakeMaterialsParser::testUnknownExtraHeadingOrContent() {
    // 1. Extra content before first heading
    QString samplePreamble = QStringLiteral("Here is your output:\n") + createValidSample();
    QString err;
    auto res1 = MakeMaterialsParser::parse(samplePreamble, &err);
    QVERIFY(!res1.has_value());
    QVERIFY(err.contains(QStringLiteral("Unknown extra content")));

    // 2. Extra heading between sections
    QString sampleExtraHeading = QStringLiteral(
        "# Resume Filename\n"
        "Res\n\n"
        "# Unknown Extra Section\n"
        "Some details\n\n"
        "# Cover Letter Filename\n"
        "Cov\n\n"
        "# Optimized & Tailored Professional Title\n"
        "Title\n\n"
        "# Optimized & Tailored Professional Summary\n"
        "Summary\n\n"
        "# Optimized & Tailored Key Skills\n"
        "Skills\n\n"
        "# Optimized & Tailored Cover Letter\n"
        "Cover"
    );
    auto res2 = MakeMaterialsParser::parse(sampleExtraHeading, &err);
    QVERIFY(!res2.has_value());
    QVERIFY(err.contains(QStringLiteral("Unknown extra heading")));
}

void TestMakeMaterialsParser::testCoverLetterOverFourParagraphs() {
    QString fiveParas = QStringLiteral(
        "Para 1\n\n"
        "Para 2\n\n"
        "Para 3\n\n"
        "Para 4\n\n"
        "Para 5"
    );

    QString sample = QStringLiteral(
        "# Resume Filename\n"
        "Res\n\n"
        "# Cover Letter Filename\n"
        "Cov\n\n"
        "# Optimized & Tailored Professional Title\n"
        "Title\n\n"
        "# Optimized & Tailored Professional Summary\n"
        "Summary\n\n"
        "# Optimized & Tailored Key Skills\n"
        "Skills\n\n"
        "# Optimized & Tailored Cover Letter\n"
        "%1"
    ).arg(fiveParas);

    QString err;
    auto res = MakeMaterialsParser::parse(sample, &err);
    QVERIFY(!res.has_value());
    QVERIFY(err.contains(QStringLiteral("maximum 4 paragraphs")));
}

QTEST_MAIN(TestMakeMaterialsParser)
#include "test_make_materials_parser.moc"
