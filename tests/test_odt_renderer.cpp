#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include "OdtTemplateRenderer.h"
#include "PdfConverter.h"
#include "ProjectPaths.h"
#include <QProcess>

using namespace jobstarr;

class TestOdtRenderer : public QObject {
    Q_OBJECT

private slots:
    void testXmlEscaping();
    void testTemplateValidationSuccess();
    void testTemplateValidationMissingPlaceholder();
    void testRenderResumeAndVerifyPlaceholdersReplaced();
    void testRenderCoverLetterMultiParagraph();
    void testSourceTemplateNotMutated();
};

void TestOdtRenderer::testXmlEscaping() {
    QString raw = QStringLiteral("C++ & Python <script> \"Quotes\" 'Single'");
    QString escaped = OdtTemplateRenderer::xmlEscape(raw);
    QCOMPARE(escaped, QStringLiteral("C++ &amp; Python &lt;script&gt; &quot;Quotes&quot; &apos;Single&apos;"));
}

void TestOdtRenderer::testTemplateValidationSuccess() {
    const QString resTemplate = ProjectPaths::defaultResumeTemplatePath();
    const QString covTemplate = ProjectPaths::defaultCoverLetterTemplatePath();

    QString err;
    QVERIFY2(OdtTemplateRenderer::validateResumeTemplate(resTemplate, &err), qPrintable(err));
    QVERIFY2(OdtTemplateRenderer::validateCoverTemplate(covTemplate, &err), qPrintable(err));
}

void TestOdtRenderer::testTemplateValidationMissingPlaceholder() {
    // If testing on a non-existent or invalid file
    QString err;
    QVERIFY(!OdtTemplateRenderer::validateResumeTemplate(QStringLiteral("/path/to/missing.odt"), &err));
}

void TestOdtRenderer::testRenderResumeAndVerifyPlaceholdersReplaced() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString resTemplate = ProjectPaths::defaultResumeTemplatePath();
    const QString outputPath = tempDir.filePath(QStringLiteral("Rendered_Resume.odt"));

    const QString title = QStringLiteral("Senior IT & Cybersecurity Analyst");
    const QString summary = QStringLiteral("Over a decade of experience.\nSecond line with special characters: <Healthcare> & SecOps.");
    const QString skills = QStringLiteral("C++, Python, Splunk, Active Directory");

    QString err;
    bool ok = OdtTemplateRenderer::renderResume(resTemplate, outputPath, title, summary, skills, &err);
    QVERIFY2(ok, qPrintable(err));
    QVERIFY(QFile::exists(outputPath));

    // Verify it is a valid zip archive
    QVERIFY(OdtTemplateRenderer::isArchiveValid(outputPath));

    // Extract content.xml and verify replacements
    QString content = OdtTemplateRenderer::extractContentXml(outputPath, &err);
    QVERIFY(!content.isEmpty());

    // Placeholders must NOT remain
    QVERIFY(!content.contains(QLatin1String(OdtTemplateRenderer::kPlaceholderTitle)));
    QVERIFY(!content.contains(QLatin1String(OdtTemplateRenderer::kPlaceholderSummary)));
    QVERIFY(!content.contains(QLatin1String(OdtTemplateRenderer::kPlaceholderSkills)));

    // Replaced values must be present and XML escaped
    QVERIFY(content.contains(QStringLiteral("Senior IT &amp; Cybersecurity Analyst")));
    QVERIFY(content.contains(QStringLiteral("&lt;Healthcare&gt; &amp; SecOps.")));
    QVERIFY(content.contains(QStringLiteral("<text:line-break/>")));
    QVERIFY(content.contains(QStringLiteral("C++, Python, Splunk, Active Directory")));
}

void TestOdtRenderer::testRenderCoverLetterMultiParagraph() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString covTemplate = ProjectPaths::defaultCoverLetterTemplatePath();
    const QString outputPath = tempDir.filePath(QStringLiteral("Rendered_Cover.odt"));

    const QString title = QStringLiteral("Security Analyst");
    const QString date = QStringLiteral("October 4, 2026");
    const QString address = QStringLiteral("123 Tech Way, Suite 100, New York, NY 10001");
    const QString company = QStringLiteral("Cyberdyne Systems");
    const QString coverBody = QStringLiteral(
        "First paragraph introducing interest in Cyberdyne.\n\n"
        "Second paragraph detailing enterprise support & threat response.\n\n"
        "Third paragraph concluding with enthusiasm."
    );

    QString err;
    bool ok = OdtTemplateRenderer::renderCoverLetter(covTemplate, outputPath, title, date, address, company, coverBody, &err);
    QVERIFY2(ok, qPrintable(err));
    QVERIFY(QFile::exists(outputPath));
    QVERIFY(OdtTemplateRenderer::isArchiveValid(outputPath));

    QString content = OdtTemplateRenderer::extractContentXml(outputPath, &err);
    QVERIFY(!content.isEmpty());

    // Verify all placeholders were replaced
    QVERIFY(!content.contains(QLatin1String(OdtTemplateRenderer::kPlaceholderTitle)));
    QVERIFY(!content.contains(QLatin1String(OdtTemplateRenderer::kPlaceholderDate)));
    QVERIFY(!content.contains(QLatin1String(OdtTemplateRenderer::kPlaceholderAddress)));
    QVERIFY(!content.contains(QLatin1String(OdtTemplateRenderer::kPlaceholderCompany)));
    QVERIFY(!content.contains(QLatin1String(OdtTemplateRenderer::kPlaceholderCoverBody)));

    // Verify multi-paragraph structure was generated
    QVERIFY(content.contains(QStringLiteral("First paragraph introducing interest in Cyberdyne.")));
    QVERIFY(content.contains(QStringLiteral("Second paragraph detailing enterprise support &amp; threat response.")));
    QVERIFY(content.contains(QStringLiteral("Third paragraph concluding with enthusiasm.")));

    // Must NOT contain P1 master page style separator which triggers a page break
    QVERIFY(!content.contains(QStringLiteral("<text:p text:style-name=\"P1\"/>")));
    QVERIFY(content.contains(QStringLiteral("<text:p text:style-name=\"P3\"/>")));

    // When LibreOffice is available, verify the rendered document converts to a 1-page PDF
    if (!PdfConverter::findConverterExecutable().isEmpty()) {
        PdfConverter converter;
        QString pdfPath;
        QString convErr;
        QVERIFY2(converter.convertToPdf(outputPath, tempDir.path(), &pdfPath, &convErr), qPrintable(convErr));
        QVERIFY(QFile::exists(pdfPath));

        QProcess proc;
        proc.start(QStringLiteral("pdfinfo"), {pdfPath});
        if (proc.waitForFinished(5000)) {
            QString infoOut = QString::fromUtf8(proc.readAllStandardOutput());
            QVERIFY(infoOut.contains(QStringLiteral("Pages:           1")));
        }
    }
}

void TestOdtRenderer::testSourceTemplateNotMutated() {
    const QString resTemplate = ProjectPaths::defaultResumeTemplatePath();
    QFileInfo beforeInfo(resTemplate);
    const qint64 beforeSize = beforeInfo.size();

    QTemporaryDir tempDir;
    const QString outputPath = tempDir.filePath(QStringLiteral("Rendered.odt"));
    OdtTemplateRenderer::renderResume(resTemplate, outputPath, "Title", "Summary", "Skills");

    QFileInfo afterInfo(resTemplate);
    QCOMPARE(afterInfo.size(), beforeSize);
}

QTEST_MAIN(TestOdtRenderer)
#include "test_odt_renderer.moc"
