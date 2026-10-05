#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include "PdfConverter.h"

using namespace jobstarr;

class MockPdfRunner : public IPdfProcessRunner {
public:
    int exitCodeToReturn{0};
    bool shouldTimeout{false};
    QString stderrToReturn;
    bool createOutputFile{true};
    bool createZeroByteFile{false};

    bool run(const QString &program,
             const QStringList &arguments,
             int timeoutMs,
             int *exitCode,
             QString *stdErr) override {
        Q_UNUSED(program);
        Q_UNUSED(timeoutMs);

        if (shouldTimeout) {
            if (stdErr) *stdErr = QStringLiteral("Process timed out.");
            return false;
        }

        if (exitCode) {
            *exitCode = exitCodeToReturn;
        }
        if (stdErr) {
            *stdErr = stderrToReturn;
        }

        if (exitCodeToReturn == 0 && createOutputFile) {
            // Find --outdir in arguments
            int outdirIdx = arguments.indexOf(QStringLiteral("--outdir"));
            if (outdirIdx != -1 && outdirIdx + 1 < arguments.size()) {
                QString outdir = arguments[outdirIdx + 1];
                QString odtPath = arguments.last();
                QFileInfo fi(odtPath);
                QString pdfPath = QDir(outdir).filePath(fi.completeBaseName() + QStringLiteral(".pdf"));
                QFile f(pdfPath);
                if (f.open(QIODevice::WriteOnly)) {
                    if (!createZeroByteFile) {
                        f.write("%PDF-1.4 mock content");
                    }
                    f.close();
                }
            }
        }

        return (exitCodeToReturn == 0);
    }
};

class TestPdfConverter : public QObject {
    Q_OBJECT

private slots:
    void testMockSuccess();
    void testMockNonZeroExit();
    void testMockTimeout();
    void testMockOutputMissing();
    void testMockZeroByteOutput();
    void testRealLibreOfficeIntegration();
};

void TestPdfConverter::testMockSuccess() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString odtPath = tempDir.filePath(QStringLiteral("test.odt"));
    QFile f(odtPath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("mock odt");
    f.close();

    auto mock = std::make_shared<MockPdfRunner>();
    PdfConverter converter(mock);

    QString pdfPath;
    QString err;
    bool ok = converter.convertToPdf(odtPath, tempDir.path(), &pdfPath, &err);
    QVERIFY2(ok, qPrintable(err));
    QVERIFY(QFile::exists(pdfPath));
}

void TestPdfConverter::testMockNonZeroExit() {
    QTemporaryDir tempDir;
    QString odtPath = tempDir.filePath(QStringLiteral("test.odt"));
    QFile f(odtPath);
    f.open(QIODevice::WriteOnly);
    f.write("mock");
    f.close();

    auto mock = std::make_shared<MockPdfRunner>();
    mock->exitCodeToReturn = 1;
    mock->stderrToReturn = QStringLiteral("Fatal error in filter export");

    PdfConverter converter(mock);
    QString pdfPath;
    QString err;
    bool ok = converter.convertToPdf(odtPath, tempDir.path(), &pdfPath, &err);
    QVERIFY(!ok);
    QVERIFY(err.contains(QStringLiteral("Fatal error in filter export")));
}

void TestPdfConverter::testMockTimeout() {
    QTemporaryDir tempDir;
    QString odtPath = tempDir.filePath(QStringLiteral("test.odt"));
    QFile f(odtPath);
    f.open(QIODevice::WriteOnly);
    f.write("mock");
    f.close();

    auto mock = std::make_shared<MockPdfRunner>();
    mock->shouldTimeout = true;

    PdfConverter converter(mock);
    QString pdfPath;
    QString err;
    bool ok = converter.convertToPdf(odtPath, tempDir.path(), &pdfPath, &err);
    QVERIFY(!ok);
    QVERIFY(err.contains(QStringLiteral("timed out")));
}

void TestPdfConverter::testMockOutputMissing() {
    QTemporaryDir tempDir;
    QString odtPath = tempDir.filePath(QStringLiteral("test.odt"));
    QFile f(odtPath);
    f.open(QIODevice::WriteOnly);
    f.write("mock");
    f.close();

    auto mock = std::make_shared<MockPdfRunner>();
    mock->exitCodeToReturn = 0;
    mock->createOutputFile = false;

    PdfConverter converter(mock);
    QString pdfPath;
    QString err;
    bool ok = converter.convertToPdf(odtPath, tempDir.path(), &pdfPath, &err);
    QVERIFY(!ok);
    QVERIFY(err.contains(QStringLiteral("does not exist")));
}

void TestPdfConverter::testMockZeroByteOutput() {
    QTemporaryDir tempDir;
    QString odtPath = tempDir.filePath(QStringLiteral("test.odt"));
    QFile f(odtPath);
    f.open(QIODevice::WriteOnly);
    f.write("mock");
    f.close();

    auto mock = std::make_shared<MockPdfRunner>();
    mock->exitCodeToReturn = 0;
    mock->createZeroByteFile = true;

    PdfConverter converter(mock);
    QString pdfPath;
    QString err;
    bool ok = converter.convertToPdf(odtPath, tempDir.path(), &pdfPath, &err);
    QVERIFY(!ok);
    QVERIFY(err.contains(QStringLiteral("0 bytes")));
}

void TestPdfConverter::testRealLibreOfficeIntegration() {
    QString converterExe = PdfConverter::findConverterExecutable();
    if (converterExe.isEmpty()) {
        QSKIP("LibreOffice not available on this system");
    }

    QTemporaryDir tempDir;
    // Copy real template
    QString realTemplate = QStringLiteral(JOBSTARR_SOURCE_DIR "/templates/Candidate_Cover000-TEMPLATE.odt");
    QString testOdt = tempDir.filePath(QStringLiteral("IntegrationTest.odt"));
    QVERIFY(QFile::copy(realTemplate, testOdt));

    PdfConverter realConverter;
    QString pdfPath;
    QString err;
    bool ok = realConverter.convertToPdf(testOdt, tempDir.path(), &pdfPath, &err);
    QVERIFY2(ok, qPrintable(err));
    QVERIFY(QFile::exists(pdfPath));
    QVERIFY(QFileInfo(pdfPath).size() > 1000);
}

QTEST_MAIN(TestPdfConverter)
#include "test_pdf_converter.moc"
