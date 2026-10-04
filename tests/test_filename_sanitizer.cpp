#include <QTest>
#include "FilenameSanitizer.h"
#include <QDir>

using namespace jobstarr;

class TestFilenameSanitizer : public QObject {
    Q_OBJECT

private slots:
    void testNormalName();
    void testHostilePathTraversal();
    void testEtcPasswd();
    void testRelativeParentPrefix();
    void testExtensionStripping();
    void testQuotedString();
    void testPathSeparators();
    void testDotAndDoubleDot();
    void testEmptyString();
    void testVeryLongName();
    void testSafeBasenameValidation();
};

void TestFilenameSanitizer::testNormalName() {
    auto opt = FilenameSanitizer::sanitize(QStringLiteral("Candidate_Resume_Acme"));
    QVERIFY(opt.has_value());
    QCOMPARE(opt.value(), QStringLiteral("Candidate_Resume_Acme"));
    QVERIFY(FilenameSanitizer::isSafeBasename(opt.value()));
}

void TestFilenameSanitizer::testHostilePathTraversal() {
    auto opt = FilenameSanitizer::sanitize(QStringLiteral("../../evil"));
    QVERIFY(opt.has_value());
    QCOMPARE(opt.value(), QStringLiteral("evil"));
    QVERIFY(FilenameSanitizer::isSafeBasename(opt.value()));

    // Verify it cannot escape output directory
    QDir outDir(QStringLiteral("/home/user/jobStarr/output"));
    QString target = outDir.filePath(opt.value() + QStringLiteral(".odt"));
    QCOMPARE(outDir.relativeFilePath(target), QStringLiteral("evil.odt"));
}

void TestFilenameSanitizer::testEtcPasswd() {
    auto opt = FilenameSanitizer::sanitize(QStringLiteral("/etc/passwd"));
    QVERIFY(opt.has_value());
    QCOMPARE(opt.value(), QStringLiteral("etc_passwd"));
    QVERIFY(FilenameSanitizer::isSafeBasename(opt.value()));

    QDir outDir(QStringLiteral("/home/user/jobStarr/output"));
    QString target = outDir.filePath(opt.value() + QStringLiteral(".odt"));
    QCOMPARE(outDir.relativeFilePath(target), QStringLiteral("etc_passwd.odt"));
}

void TestFilenameSanitizer::testRelativeParentPrefix() {
    auto opt = FilenameSanitizer::sanitize(QStringLiteral("../resume"));
    QVERIFY(opt.has_value());
    QCOMPARE(opt.value(), QStringLiteral("resume"));
    QVERIFY(FilenameSanitizer::isSafeBasename(opt.value()));
}

void TestFilenameSanitizer::testExtensionStripping() {
    auto optOdt = FilenameSanitizer::sanitize(QStringLiteral("resume.odt"));
    QVERIFY(optOdt.has_value());
    QCOMPARE(optOdt.value(), QStringLiteral("resume"));

    auto optPdf = FilenameSanitizer::sanitize(QStringLiteral("resume.pdf"));
    QVERIFY(optPdf.has_value());
    QCOMPARE(optPdf.value(), QStringLiteral("resume"));

    auto optUpper = FilenameSanitizer::sanitize(QStringLiteral("resume.ODT"));
    QVERIFY(optUpper.has_value());
    QCOMPARE(optUpper.value(), QStringLiteral("resume"));
}

void TestFilenameSanitizer::testQuotedString() {
    auto optDouble = FilenameSanitizer::sanitize(QStringLiteral("\"resume\""));
    QVERIFY(optDouble.has_value());
    QCOMPARE(optDouble.value(), QStringLiteral("resume"));

    auto optSingle = FilenameSanitizer::sanitize(QStringLiteral("'resume'"));
    QVERIFY(optSingle.has_value());
    QCOMPARE(optSingle.value(), QStringLiteral("resume"));
}

void TestFilenameSanitizer::testPathSeparators() {
    auto optSlash = FilenameSanitizer::sanitize(QStringLiteral("resume/name"));
    QVERIFY(optSlash.has_value());
    QCOMPARE(optSlash.value(), QStringLiteral("resume_name"));

    auto optBackslash = FilenameSanitizer::sanitize(QStringLiteral("resume\\name"));
    QVERIFY(optBackslash.has_value());
    QCOMPARE(optBackslash.value(), QStringLiteral("resume_name"));
}

void TestFilenameSanitizer::testDotAndDoubleDot() {
    QString err;
    auto optDot = FilenameSanitizer::sanitize(QStringLiteral("."), &err);
    QVERIFY(!optDot.has_value());

    auto optDotDot = FilenameSanitizer::sanitize(QStringLiteral(".."), &err);
    QVERIFY(!optDotDot.has_value());
}

void TestFilenameSanitizer::testEmptyString() {
    QString err;
    auto optEmpty = FilenameSanitizer::sanitize(QStringLiteral("   "), &err);
    QVERIFY(!optEmpty.has_value());
}

void TestFilenameSanitizer::testVeryLongName() {
    QString longName = QStringLiteral("A").repeated(200);
    auto opt = FilenameSanitizer::sanitize(longName);
    QVERIFY(opt.has_value());
    QCOMPARE(opt.value().length(), 120);
    QVERIFY(FilenameSanitizer::isSafeBasename(opt.value()));
}

void TestFilenameSanitizer::testSafeBasenameValidation() {
    QVERIFY(FilenameSanitizer::isSafeBasename(QStringLiteral("Candidate_Resume_Acme")));
    QVERIFY(!FilenameSanitizer::isSafeBasename(QStringLiteral("../evil")));
    QVERIFY(!FilenameSanitizer::isSafeBasename(QStringLiteral("/etc/passwd")));
    QVERIFY(!FilenameSanitizer::isSafeBasename(QStringLiteral("resume/name")));
    QVERIFY(!FilenameSanitizer::isSafeBasename(QStringLiteral(".")));
    QVERIFY(!FilenameSanitizer::isSafeBasename(QStringLiteral("..")));
    QVERIFY(!FilenameSanitizer::isSafeBasename(QString()));
}

QTEST_MAIN(TestFilenameSanitizer)
#include "test_filename_sanitizer.moc"
