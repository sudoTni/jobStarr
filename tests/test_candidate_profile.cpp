#include <QTest>
#include "CandidateProfile.h"

using namespace jobstarr;

class TestCandidateProfile : public QObject {
    Q_OBJECT

private slots:
    void testAlphabetizeNewlineSeparated();
    void testAlphabetizeCommaSeparated();
    void testAlphabetizeMarkdownBullets();
    void testCaseInsensitiveDeduplication();
    void testTestimonialsOptional();
    void testInvalidProfileMissingRequired();
};

void TestCandidateProfile::testAlphabetizeNewlineSeparated() {
    QString raw = QStringLiteral("Python\nC++\nQt\nLinux");
    QString normalized = CandidateProfile::normalizeKeySkills(raw);
    QCOMPARE(normalized, QStringLiteral("C++, Linux, Python, Qt"));
}

void TestCandidateProfile::testAlphabetizeCommaSeparated() {
    QString raw = QStringLiteral("Python, C++, Qt, Linux");
    QString normalized = CandidateProfile::normalizeKeySkills(raw);
    QCOMPARE(normalized, QStringLiteral("C++, Linux, Python, Qt"));
}

void TestCandidateProfile::testAlphabetizeMarkdownBullets() {
    QString raw = QStringLiteral("- Python\n- C++\n* Qt\n• Linux");
    QString normalized = CandidateProfile::normalizeKeySkills(raw);
    QCOMPARE(normalized, QStringLiteral("C++, Linux, Python, Qt"));
}

void TestCandidateProfile::testCaseInsensitiveDeduplication() {
    QString raw = QStringLiteral("Python, python, PYTHON, C++, c++");
    QString normalized = CandidateProfile::normalizeKeySkills(raw);
    QCOMPARE(normalized, QStringLiteral("C++, Python"));
}

void TestCandidateProfile::testTestimonialsOptional() {
    CandidateProfile profile;
    profile.professionalTitle = QStringLiteral("Title");
    profile.professionalSummary = QStringLiteral("Summary");
    profile.keySkills = QStringLiteral("C++, Python");
    profile.resume = QStringLiteral("Resume");
    profile.testimonials = QString(); // Empty testimonials is valid

    QString err;
    QVERIFY2(profile.isValid(&err), qPrintable(err));
}

void TestCandidateProfile::testInvalidProfileMissingRequired() {
    CandidateProfile p1;
    p1.professionalSummary = QStringLiteral("Summary");
    p1.keySkills = QStringLiteral("Skills");
    p1.resume = QStringLiteral("Resume");
    QVERIFY(!p1.isValid()); // missing title

    CandidateProfile p2;
    p2.professionalTitle = QStringLiteral("Title");
    p2.keySkills = QStringLiteral("Skills");
    p2.resume = QStringLiteral("Resume");
    QVERIFY(!p2.isValid()); // missing summary

    CandidateProfile p3;
    p3.professionalTitle = QStringLiteral("Title");
    p3.professionalSummary = QStringLiteral("Summary");
    p3.resume = QStringLiteral("Resume");
    QVERIFY(!p3.isValid()); // missing skills

    CandidateProfile p4;
    p4.professionalTitle = QStringLiteral("Title");
    p4.professionalSummary = QStringLiteral("Summary");
    p4.keySkills = QStringLiteral("Skills");
    QVERIFY(!p4.isValid()); // missing resume
}

QTEST_MAIN(TestCandidateProfile)
#include "test_candidate_profile.moc"
