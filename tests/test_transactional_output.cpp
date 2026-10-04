#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include "OutputManager.h"
#include "OdtTemplateRenderer.h"
#include "ProjectPaths.h"

using namespace jobstarr;

class TestTransactionalOutput : public QObject {
    Q_OBJECT

private slots:
    void testSuccessfulPublishPublishesAllFourFiles();
    void testMissingStagedFilePublishesNothing();
    void testCorruptedOdtPublishesNothing();
    void testPreviousSuccessfulFilesSurviveFailedRegeneration();
};

void TestTransactionalOutput::testSuccessfulPublishPublishesAllFourFiles() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    OutputManager outMgr(tempDir.path());
    QString err;
    QString stagingDir = outMgr.createStagingDirectory(&err);
    QVERIFY(!stagingDir.isEmpty());

    const QString resBase = QStringLiteral("Resume_Test");
    const QString covBase = QStringLiteral("Cover_Test");

    // Copy real template files into staging so they are valid ODTs
    const QString resTemplate = ProjectPaths::defaultResumeTemplatePath();
    const QString covTemplate = ProjectPaths::defaultCoverLetterTemplatePath();

    QVERIFY(QFile::copy(resTemplate, QDir(stagingDir).filePath(resBase + QStringLiteral(".odt"))));
    QVERIFY(QFile::copy(covTemplate, QDir(stagingDir).filePath(covBase + QStringLiteral(".odt"))));

    // Create non-empty mock PDFs
    QFile f1(QDir(stagingDir).filePath(resBase + QStringLiteral(".pdf")));
    QVERIFY(f1.open(QIODevice::WriteOnly));
    f1.write("%PDF-1.4 test");
    f1.close();

    QFile f2(QDir(stagingDir).filePath(covBase + QStringLiteral(".pdf")));
    QVERIFY(f2.open(QIODevice::WriteOnly));
    f2.write("%PDF-1.4 test");
    f2.close();

    QStringList published;
    bool ok = outMgr.publishStagedArtifacts(stagingDir, resBase, covBase, &published, &err);
    QVERIFY2(ok, qPrintable(err));
    QCOMPARE(published.size(), 4);

    // Verify all 4 exist in final output directory
    QVERIFY(QFile::exists(QDir(tempDir.path()).filePath(resBase + QStringLiteral(".odt"))));
    QVERIFY(QFile::exists(QDir(tempDir.path()).filePath(resBase + QStringLiteral(".pdf"))));
    QVERIFY(QFile::exists(QDir(tempDir.path()).filePath(covBase + QStringLiteral(".odt"))));
    QVERIFY(QFile::exists(QDir(tempDir.path()).filePath(covBase + QStringLiteral(".pdf"))));

    // Verify staging directory was cleaned up
    QVERIFY(!QDir(stagingDir).exists());
}

void TestTransactionalOutput::testMissingStagedFilePublishesNothing() {
    QTemporaryDir tempDir;
    OutputManager outMgr(tempDir.path());

    QString stagingDir = outMgr.createStagingDirectory();
    const QString resBase = QStringLiteral("Resume_Test");
    const QString covBase = QStringLiteral("Cover_Test");

    // Only create 3 files (missing cover pdf)
    const QString resTemplate = ProjectPaths::defaultResumeTemplatePath();
    QFile::copy(resTemplate, QDir(stagingDir).filePath(resBase + QStringLiteral(".odt")));
    QFile::copy(resTemplate, QDir(stagingDir).filePath(covBase + QStringLiteral(".odt")));

    QFile f1(QDir(stagingDir).filePath(resBase + QStringLiteral(".pdf")));
    f1.open(QIODevice::WriteOnly);
    f1.write("%PDF-1.4");
    f1.close();

    QString err;
    bool ok = outMgr.publishStagedArtifacts(stagingDir, resBase, covBase, nullptr, &err);
    QVERIFY(!ok);
    QVERIFY(err.contains(QStringLiteral("missing")));

    // Destination must NOT have published the partial files
    QVERIFY(!QFile::exists(QDir(tempDir.path()).filePath(resBase + QStringLiteral(".odt"))));

    outMgr.cleanStagingDirectory(stagingDir);
}

void TestTransactionalOutput::testCorruptedOdtPublishesNothing() {
    QTemporaryDir tempDir;
    OutputManager outMgr(tempDir.path());

    QString stagingDir = outMgr.createStagingDirectory();
    const QString resBase = QStringLiteral("Resume_Test");
    const QString covBase = QStringLiteral("Cover_Test");

    // Write corrupted non-zip bytes to resume.odt
    QFile fOdt(QDir(stagingDir).filePath(resBase + QStringLiteral(".odt")));
    fOdt.open(QIODevice::WriteOnly);
    fOdt.write("corrupted bytes not zip");
    fOdt.close();

    const QString covTemplate = ProjectPaths::defaultCoverLetterTemplatePath();
    QFile::copy(covTemplate, QDir(stagingDir).filePath(covBase + QStringLiteral(".odt")));

    QFile f1(QDir(stagingDir).filePath(resBase + QStringLiteral(".pdf")));
    f1.open(QIODevice::WriteOnly);
    f1.write("%PDF-1.4");
    f1.close();

    QFile f2(QDir(stagingDir).filePath(covBase + QStringLiteral(".pdf")));
    f2.open(QIODevice::WriteOnly);
    f2.write("%PDF-1.4");
    f2.close();

    QString err;
    bool ok = outMgr.publishStagedArtifacts(stagingDir, resBase, covBase, nullptr, &err);
    QVERIFY(!ok);
    QVERIFY(err.contains(QStringLiteral("corrupt")));

    // Destination remains clean
    QVERIFY(!QFile::exists(QDir(tempDir.path()).filePath(covBase + QStringLiteral(".odt"))));

    outMgr.cleanStagingDirectory(stagingDir);
}

void TestTransactionalOutput::testPreviousSuccessfulFilesSurviveFailedRegeneration() {
    QTemporaryDir tempDir;
    OutputManager outMgr(tempDir.path());

    // Create pre-existing good files in output
    const QString goodResume = QDir(tempDir.path()).filePath(QStringLiteral("Previous_Good.odt"));
    QFile g1(goodResume);
    QVERIFY(g1.open(QIODevice::WriteOnly));
    g1.write("good existing data");
    g1.close();

    // Now simulate failed staging
    QString stagingDir = outMgr.createStagingDirectory();
    // Missing files in staging
    QString err;
    bool ok = outMgr.publishStagedArtifacts(stagingDir, QStringLiteral("New_Resume"), QStringLiteral("New_Cover"), nullptr, &err);
    QVERIFY(!ok);

    // Verify pre-existing file in output was NOT touched
    QVERIFY(QFile::exists(goodResume));
    QFile checkG1(goodResume);
    QVERIFY(checkG1.open(QIODevice::ReadOnly));
    QCOMPARE(checkG1.readAll(), QByteArray("good existing data"));

    outMgr.cleanStagingDirectory(stagingDir);
}

QTEST_MAIN(TestTransactionalOutput)
#include "test_transactional_output.moc"
