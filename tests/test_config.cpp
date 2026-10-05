#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include "ConfigManager.h"
#include <sys/stat.h>

using namespace jobstarr;

class TestConfig : public QObject {
    Q_OBJECT

private slots:
    void testMissingYamlLoadsDefaults();
    void testSaveAndReloadRoundTrip();
    void testMultilinePreservation();
    void testFilePermissions();
    void testConfigMigrationFromV010();
};

void TestConfig::testMissingYamlLoadsDefaults() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString nonExistentPath = tempDir.path() + QStringLiteral("/non_existent_config.yaml");
    ConfigManager manager(nonExistentPath);

    AppConfig config;
    QString err;
    QVERIFY2(manager.load(config, &err), qPrintable(err));

    QCOMPARE(config.version, QStringLiteral("0.2.1"));
    QCOMPARE(config.timeoutSeconds, 300);
    QCOMPARE(config.searchTimeoutSeconds, 300);
    QVERIFY(!config.systemPrompt.isEmpty());
    QVERIFY(config.systemPrompt.contains(QStringLiteral("Veritas")));

    QVERIFY(!config.jobJudgePrompt.isEmpty());
    QVERIFY(config.jobJudgePrompt.contains(QStringLiteral("{targJD}")));
    QVERIFY(config.jobJudgePrompt.contains(QStringLiteral("{myResume}")));

    QVERIFY(!config.makeMaterialsPrompt.isEmpty());
    QVERIFY(config.makeMaterialsPrompt.contains(QStringLiteral("{myProfessionalTitle}")));
    QVERIFY(config.makeMaterialsPrompt.contains(QStringLiteral("{myProfessionalSummary}")));
    QVERIFY(config.makeMaterialsPrompt.contains(QStringLiteral("{myKeySkills}")));

    QVERIFY(!config.myResume.isEmpty());
    QVERIFY(config.myResume.contains(QStringLiteral("Candidate")));

    QVERIFY(!config.professionalTitle.isEmpty());
    QVERIFY(!config.professionalSummary.isEmpty());
    QVERIFY(!config.keySkills.isEmpty());
    QVERIFY(!config.resumeTemplatePath.isEmpty());
    QVERIFY(!config.coverLetterTemplatePath.isEmpty());
}

void TestConfig::testSaveAndReloadRoundTrip() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString testPath = tempDir.path() + QStringLiteral("/jobStarr.yaml");
    ConfigManager manager(testPath);

    AppConfig original;
    original.version = QStringLiteral("0.2.1");
    original.apiEndpoint = QStringLiteral("https://api.myllm.com/v1/chat/completions");
    original.model = QStringLiteral("custom-llm-v1");
    original.apiKey = QStringLiteral("secret-token-12345");
    original.reasoningEffort = QStringLiteral("high");
    original.timeoutSeconds = 450;

    original.searchEndpoint = QStringLiteral("https://openrouter.ai/api/v1/chat/completions");
    original.searchModel = QStringLiteral("sonar");
    original.searchApiKey = QStringLiteral("search-secret");
    original.searchReasoningEffort = QStringLiteral("medium");
    original.searchTimeoutSeconds = 480;

    original.systemPrompt = QStringLiteral("Custom system prompt.\nLine 2.\nLine 3.");
    original.jobJudgePrompt = QStringLiteral("Evaluate {targJD} against {myResume} please.\nMultiple lines here.");
    original.makeMaterialsPrompt = QStringLiteral("Make materials for {myProfessionalTitle} {myProfessionalSummary} {myKeySkills} {targJD} {myResume} {myTestimonials}");

    original.professionalTitle = QStringLiteral("Lead Security Engineer");
    original.professionalSummary = QStringLiteral("Experienced cybersecurity engineer.");
    original.keySkills = QStringLiteral("C++, Python, Splunk");
    original.myResume = QStringLiteral("# Resume\n- Point A\n- Point B\nSummary details.");
    original.testimonials = QStringLiteral("Highly recommended engineer.");

    original.resumeTemplatePath = QStringLiteral("/templates/Custom_Resume.odt");
    original.coverLetterTemplatePath = QStringLiteral("/templates/Custom_Cover.odt");

    QString saveErr;
    QVERIFY2(manager.save(original, &saveErr), qPrintable(saveErr));
    QVERIFY(QFile::exists(testPath));

    AppConfig loaded;
    QString loadErr;
    QVERIFY2(manager.load(loaded, &loadErr), qPrintable(loadErr));

    QCOMPARE(loaded.version, original.version);
    QCOMPARE(loaded.apiEndpoint, original.apiEndpoint);
    QCOMPARE(loaded.model, original.model);
    QCOMPARE(loaded.apiKey, original.apiKey);
    QCOMPARE(loaded.reasoningEffort, original.reasoningEffort);
    QCOMPARE(loaded.timeoutSeconds, original.timeoutSeconds);

    QCOMPARE(loaded.searchEndpoint, original.searchEndpoint);
    QCOMPARE(loaded.searchModel, original.searchModel);
    QCOMPARE(loaded.searchApiKey, original.searchApiKey);
    QCOMPARE(loaded.searchReasoningEffort, original.searchReasoningEffort);
    QCOMPARE(loaded.searchTimeoutSeconds, original.searchTimeoutSeconds);

    QCOMPARE(loaded.systemPrompt.trimmed(), original.systemPrompt.trimmed());
    QCOMPARE(loaded.jobJudgePrompt.trimmed(), original.jobJudgePrompt.trimmed());
    QCOMPARE(loaded.makeMaterialsPrompt.trimmed(), original.makeMaterialsPrompt.trimmed());

    QCOMPARE(loaded.professionalTitle, original.professionalTitle);
    QCOMPARE(loaded.professionalSummary.trimmed(), original.professionalSummary.trimmed());
    QCOMPARE(loaded.keySkills.trimmed(), original.keySkills.trimmed());
    QCOMPARE(loaded.myResume.trimmed(), original.myResume.trimmed());
    QCOMPARE(loaded.testimonials.trimmed(), original.testimonials.trimmed());

    QCOMPARE(loaded.resumeTemplatePath, original.resumeTemplatePath);
    QCOMPARE(loaded.coverLetterTemplatePath, original.coverLetterTemplatePath);
}

void TestConfig::testMultilinePreservation() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString testPath = tempDir.path() + QStringLiteral("/jobStarr.yaml");
    ConfigManager manager(testPath);

    AppConfig original;
    original.systemPrompt = QStringLiteral("Line 1\n\nLine 3 with Markdown *bold* and _italic_\n\n- bullet 1\n- bullet 2\n");
    original.jobJudgePrompt = QStringLiteral("### Heading\n\nData:\n{targJD}\n\nCandidate:\n{myResume}\n\nConclusion:\nFinal thoughts.");
    original.myResume = QStringLiteral("Candidate\nAnytown, USA\n\n12+ years experience:\n* Splunk\n* Carbon Black\n");

    QString err;
    QVERIFY2(manager.save(original, &err), qPrintable(err));

    AppConfig loaded;
    QVERIFY2(manager.load(loaded, &err), qPrintable(err));

    QCOMPARE(loaded.systemPrompt.trimmed(), original.systemPrompt.trimmed());
    QCOMPARE(loaded.jobJudgePrompt.trimmed(), original.jobJudgePrompt.trimmed());
    QCOMPARE(loaded.myResume.trimmed(), original.myResume.trimmed());
}

void TestConfig::testFilePermissions() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString testPath = tempDir.path() + QStringLiteral("/jobStarr.yaml");
    ConfigManager manager(testPath);

    AppConfig config;
    config.apiKey = QStringLiteral("secret-key");
    QString err;
    QVERIFY2(manager.save(config, &err), qPrintable(err));

    struct stat st;
    int res = stat(testPath.toLocal8Bit().constData(), &st);
    QCOMPARE(res, 0);

    mode_t permissions = st.st_mode & 0777;
    QVERIFY2((permissions & 0077) == 0, qPrintable(QString("Expected 0600 permissions, got %1").arg(QString::number(permissions, 8))));
}

void TestConfig::testConfigMigrationFromV010() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString v010Path = tempDir.filePath(QStringLiteral("jobStarr-v010.yaml"));

    // Write a genuine v0.1.0 configuration file
    const QString v010Yaml = QStringLiteral(
        "version: \"0.1.0\"\n"
        "api:\n"
        "  endpoint: \"https://api.v010.com/v1/chat/completions\"\n"
        "  model: \"v010-model\"\n"
        "  api_key: \"v010-key\"\n"
        "  timeout_seconds: 180\n"
        "prompts:\n"
        "  system: |\n"
        "    v010 system prompt\n"
        "  job_judge: |\n"
        "    v010 judge prompt {targJD} {myResume}\n"
        "candidate:\n"
        "  resume: |\n"
        "    v010 candidate resume\n"
    );

    QFile f(v010Path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(v010Yaml.toUtf8());
    f.close();

    ConfigManager manager(v010Path);
    AppConfig loaded;
    QString err;
    QVERIFY2(manager.load(loaded, &err), qPrintable(err));

    // 1. Existing Primary LLM settings retained
    QCOMPARE(loaded.apiEndpoint, QStringLiteral("https://api.v010.com/v1/chat/completions"));
    QCOMPARE(loaded.model, QStringLiteral("v010-model"));
    QCOMPARE(loaded.apiKey, QStringLiteral("v010-key"));
    QCOMPARE(loaded.timeoutSeconds, 180);

    // 2. Existing prompts & resume retained
    QCOMPARE(loaded.systemPrompt.trimmed(), QStringLiteral("v010 system prompt"));
    QCOMPARE(loaded.jobJudgePrompt.trimmed(), QStringLiteral("v010 judge prompt {targJD} {myResume}"));
    QCOMPARE(loaded.myResume.trimmed(), QStringLiteral("v010 candidate resume"));

    // 3. New Search LLM values initialized empty (Section 61)
    QVERIFY(loaded.searchEndpoint.isEmpty());
    QVERIFY(loaded.searchModel.isEmpty());
    QVERIFY(loaded.searchApiKey.isEmpty());
    QVERIFY(loaded.reasoningEffort.isEmpty());
    QVERIFY(loaded.searchReasoningEffort.isEmpty());
    QCOMPARE(loaded.searchTimeoutSeconds, 300);

    // 4. New candidate fields initialized from defaults
    QVERIFY(!loaded.professionalTitle.isEmpty());
    QVERIFY(!loaded.professionalSummary.isEmpty());
    QVERIFY(!loaded.keySkills.isEmpty());
    QVERIFY(!loaded.makeMaterialsPrompt.isEmpty());

    // 5. New template paths initialized from defaults
    QVERIFY(!loaded.resumeTemplatePath.isEmpty());
    QVERIFY(!loaded.coverLetterTemplatePath.isEmpty());

    // 6. After Save, all fields round-trip
    QString saveErr;
    QVERIFY2(manager.save(loaded, &saveErr), qPrintable(saveErr));

    AppConfig reloaded;
    QVERIFY2(manager.load(reloaded, &err), qPrintable(err));
    QCOMPARE(reloaded.version, loaded.version);
    QCOMPARE(reloaded.apiEndpoint, loaded.apiEndpoint);
    QCOMPARE(reloaded.model, loaded.model);
    QCOMPARE(reloaded.apiKey, loaded.apiKey);
    QCOMPARE(reloaded.myResume.trimmed(), loaded.myResume.trimmed());
    QCOMPARE(reloaded.professionalTitle, loaded.professionalTitle);
    QCOMPARE(reloaded.searchTimeoutSeconds, 300);
}

QTEST_MAIN(TestConfig)
#include "test_config.moc"
