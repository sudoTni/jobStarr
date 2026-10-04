#include <QTest>
#include "PromptRenderer.h"
#include "CandidateProfile.h"

using namespace jobstarr;

class TestPromptRenderer : public QObject {
    Q_OBJECT

private slots:
    void testExactReplacement();
    void testMultipleOccurrences();
    void testTemplateImmutability();
    void testMissingPlaceholders();
    void testSpecialCharacters();

    // Section 55: Make Materials Prompt Rendering Tests
    void testMakeMaterialsPromptAllPlaceholdersReplaced();
    void testMakeMaterialsPromptRepeatedOccurrences();
    void testMakeMaterialsPromptEmptyTestimonialsAccepted();
    void testMakeMaterialsPromptSpecialCharactersPreserved();
    void testMakeMaterialsPromptImmutability();
    void testMakeMaterialsPromptMissingPlaceholders();
    void testCandidateSkillsAlphabetizedAndDeduplicated();
};

void TestPromptRenderer::testExactReplacement() {
    const QString tmpl = QStringLiteral("Job:\n{targJD}\nResume:\n{myResume}\nDone.");
    const QString serializedJob = QStringLiteral("{\"title\": \"Engineer\"}");
    const QString resumeText = QStringLiteral("# Candidate\nSecurity Engineer");

    QString err;
    const QString result = PromptRenderer::render(tmpl, serializedJob, resumeText, &err);

    QVERIFY(err.isEmpty());
    QVERIFY(!result.contains(QStringLiteral("{targJD}")));
    QVERIFY(!result.contains(QStringLiteral("{myResume}")));
    QVERIFY(result.contains(serializedJob));
    QVERIFY(result.contains(resumeText));
}

void TestPromptRenderer::testMultipleOccurrences() {
    const QString tmpl = QStringLiteral("First {targJD} and second {targJD}, then {myResume} and {myResume}.");
    const QString job = QStringLiteral("JOB_DATA");
    const QString resume = QStringLiteral("RESUME_DATA");

    QString err;
    const QString result = PromptRenderer::render(tmpl, job, resume, &err);

    QVERIFY(err.isEmpty());
    QCOMPARE(result, QStringLiteral("First JOB_DATA and second JOB_DATA, then RESUME_DATA and RESUME_DATA."));
}

void TestPromptRenderer::testTemplateImmutability() {
    const QString originalTmpl = QStringLiteral("Template with {targJD} and {myResume}.");
    const QString copyTmpl = originalTmpl;

    QString err;
    PromptRenderer::render(copyTmpl, QStringLiteral("J"), QStringLiteral("R"), &err);

    QCOMPARE(copyTmpl, originalTmpl);
}

void TestPromptRenderer::testMissingPlaceholders() {
    QString err;

    // Missing both
    QVERIFY(!PromptRenderer::validateTemplate(QStringLiteral("Hello world"), &err));
    QVERIFY(err.contains(QStringLiteral("{targJD}")) && err.contains(QStringLiteral("{myResume}")));

    // Missing targJD
    err.clear();
    QVERIFY(!PromptRenderer::validateTemplate(QStringLiteral("Only {myResume} is here"), &err));
    QVERIFY(err.contains(QStringLiteral("{targJD}")));

    // Missing myResume
    err.clear();
    QVERIFY(!PromptRenderer::validateTemplate(QStringLiteral("Only {targJD} is here"), &err));
    QVERIFY(err.contains(QStringLiteral("{myResume}")));

    // Both present
    err.clear();
    QVERIFY(PromptRenderer::validateTemplate(QStringLiteral("Here is {targJD} and {myResume}"), &err));
    QVERIFY(err.isEmpty());
}

void TestPromptRenderer::testSpecialCharacters() {
    // Test regex meta-characters like $, \, \\, {0}, %, \n, \t, quotes, markdown
    const QString tmpl = QStringLiteral("Start\n{targJD}\nMiddle\n{myResume}\nEnd");
    const QString trickyJob = QStringLiteral("{\"$schema\": \"http://example.com\", \"val\": \"$100,000 /yr \\n\\t *bold* [link](url)\"}");
    const QString trickyResume = QStringLiteral("Skills: C++ \\ regex (.*?) [a-z]+ $1 \\1 & <tag> \"quoted\"");

    QString err;
    const QString result = PromptRenderer::render(tmpl, trickyJob, trickyResume, &err);

    QVERIFY(err.isEmpty());
    QVERIFY(result.contains(trickyJob));
    QVERIFY(result.contains(trickyResume));
    QVERIFY(result.startsWith(QStringLiteral("Start\n")));
    QVERIFY(result.endsWith(QStringLiteral("\nEnd")));
}

void TestPromptRenderer::testMakeMaterialsPromptAllPlaceholdersReplaced() {
    const QString tmpl = QStringLiteral(
        "Title: {myProfessionalTitle}\n"
        "Summary: {myProfessionalSummary}\n"
        "Skills: {myKeySkills}\n"
        "Job: {targJD}\n"
        "Resume: {myResume}\n"
        "Testimonials: {myTestimonials}\n"
    );

    QString err;
    QString rendered = PromptRenderer::renderMakeMaterialsPrompt(
        tmpl,
        QStringLiteral("Senior Engineer"),
        QStringLiteral("Experienced in SecOps"),
        QStringLiteral("C++, Python, Splunk"),
        QStringLiteral("{\"company\": \"Acme\"}"),
        QStringLiteral("Resume details"),
        QStringLiteral("Great coworker"),
        &err
    );

    QVERIFY2(err.isEmpty(), qPrintable(err));
    QVERIFY(!rendered.contains(QStringLiteral("{myProfessionalTitle}")));
    QVERIFY(!rendered.contains(QStringLiteral("{myProfessionalSummary}")));
    QVERIFY(!rendered.contains(QStringLiteral("{myKeySkills}")));
    QVERIFY(!rendered.contains(QStringLiteral("{targJD}")));
    QVERIFY(!rendered.contains(QStringLiteral("{myResume}")));
    QVERIFY(!rendered.contains(QStringLiteral("{myTestimonials}")));

    QVERIFY(rendered.contains(QStringLiteral("Senior Engineer")));
    QVERIFY(rendered.contains(QStringLiteral("Experienced in SecOps")));
    QVERIFY(rendered.contains(QStringLiteral("C++, Python, Splunk")));
    QVERIFY(rendered.contains(QStringLiteral("{\"company\": \"Acme\"}")));
    QVERIFY(rendered.contains(QStringLiteral("Resume details")));
    QVERIFY(rendered.contains(QStringLiteral("Great coworker")));
}

void TestPromptRenderer::testMakeMaterialsPromptRepeatedOccurrences() {
    const QString tmpl = QStringLiteral(
        "{myProfessionalTitle} {myProfessionalTitle} "
        "{myProfessionalSummary} {myKeySkills} {targJD} {myResume} {myTestimonials}"
    );

    QString err;
    QString rendered = PromptRenderer::renderMakeMaterialsPrompt(
        tmpl, "Title", "Summary", "Skills", "Job", "Resume", "Testimonials", &err
    );
    QVERIFY(err.isEmpty());
    QVERIFY(rendered.startsWith(QStringLiteral("Title Title ")));
}

void TestPromptRenderer::testMakeMaterialsPromptEmptyTestimonialsAccepted() {
    const QString tmpl = QStringLiteral(
        "T: {myProfessionalTitle} S: {myProfessionalSummary} K: {myKeySkills} J: {targJD} R: {myResume} Test: [{myTestimonials}]"
    );

    QString err;
    QString rendered = PromptRenderer::renderMakeMaterialsPrompt(
        tmpl, "Title", "Summary", "Skills", "Job", "Resume", "", &err
    );
    QVERIFY(err.isEmpty());
    QVERIFY(rendered.contains(QStringLiteral("Test: []")));
}

void TestPromptRenderer::testMakeMaterialsPromptSpecialCharactersPreserved() {
    const QString tmpl = QStringLiteral(
        "{myProfessionalTitle}\n{myProfessionalSummary}\n{myKeySkills}\n{targJD}\n{myResume}\n{myTestimonials}"
    );

    const QString specialResume = QStringLiteral("Regex: ^[a-z]+$ \\1 \\2 & <tag> $100% \"quotes\"");
    const QString specialTestimonial = QStringLiteral("\"He was #1 in response time: 100% & <fast>!\"");

    QString err;
    QString rendered = PromptRenderer::renderMakeMaterialsPrompt(
        tmpl, "Title", "Summary", "Skills", "Job", specialResume, specialTestimonial, &err
    );
    QVERIFY(err.isEmpty());
    QVERIFY(rendered.contains(specialResume));
    QVERIFY(rendered.contains(specialTestimonial));
}

void TestPromptRenderer::testMakeMaterialsPromptImmutability() {
    const QString tmpl = QStringLiteral(
        "{myProfessionalTitle} {myProfessionalSummary} {myKeySkills} {targJD} {myResume} {myTestimonials}"
    );
    const QString tmplCopy = tmpl;

    PromptRenderer::renderMakeMaterialsPrompt(tmplCopy, "T", "S", "K", "J", "R", "Test");
    QCOMPARE(tmplCopy, tmpl);
}

void TestPromptRenderer::testMakeMaterialsPromptMissingPlaceholders() {
    const QString incomplete = QStringLiteral(
        "{myProfessionalTitle} {myProfessionalSummary} {myKeySkills} {targJD} {myResume}" // missing {myTestimonials}
    );

    QString err;
    QVERIFY(!PromptRenderer::validateMakeMaterialsTemplate(incomplete, &err));
    QVERIFY(err.contains(QStringLiteral("{myTestimonials}")));
}

void TestPromptRenderer::testCandidateSkillsAlphabetizedAndDeduplicated() {
    QString rawSkills = QStringLiteral("Splunk, Active Directory, splunk, Carbon Black, python, Python");
    QString alphabetized = CandidateProfile::normalizeKeySkills(rawSkills);
    QCOMPARE(alphabetized, QStringLiteral("Active Directory, Carbon Black, Python, Splunk"));
}

QTEST_MAIN(TestPromptRenderer)
#include "test_prompt_renderer.moc"
