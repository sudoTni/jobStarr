#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSignalSpy>
#include "JobSpyClient.h"

using namespace jobstarr;

class TestScraperBoundary : public QObject {
    Q_OBJECT

private slots:
    void testSuccessfulScrape();
    void testNonzeroExitStderr();
    void testMalformedJson();
    void testTimeout();
};

void TestScraperBoundary::testSuccessfulScrape() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString mockScript = tempDir.path() + QStringLiteral("/jobspy_bridge.py");
    QFile file(mockScript);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(
        "#!/usr/bin/env python3\n"
        "import json, sys\n"
        "job = {\n"
        "  'source': 'linkedin',\n"
        "  'id': 'li-999999',\n"
        "  'title': 'Staff Security Engineer',\n"
        "  'company_name': 'Acme Corp',\n"
        "  'company_url': 'https://linkedin.com/company/acme',\n"
        "  'company_url_direct': None,\n"
        "  'location': {'city': 'New York', 'state': 'NY', 'country': 'USA'},\n"
        "  'date_posted': '2026-10-01',\n"
        "  'job_url': 'https://linkedin.com/jobs/view/999999',\n"
        "  'job_url_direct': None,\n"
        "  'is_remote': True,\n"
        "  'job_type': ['fulltime'],\n"
        "  'job_level': 'Senior',\n"
        "  'job_function': 'Engineering',\n"
        "  'listing_type': None,\n"
        "  'compensation': None,\n"
        "  'description': 'Great job description',\n"
        "  'emails': [],\n"
        "  'company_industry': 'Technology',\n"
        "  'company_addresses': None,\n"
        "  'company_num_employees': None,\n"
        "  'company_revenue': None,\n"
        "  'company_description': None,\n"
        "  'company_logo': None\n"
        "}\n"
        "sys.stdout.write(json.dumps(job) + '\\n')\n"
        "sys.exit(0)\n"
    );
    file.close();
    QFile::setPermissions(mockScript, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    qputenv("JOBSTARR_BRIDGE_PATH", mockScript.toLocal8Bit());

    JobSpyClient client;
    QSignalSpy successSpy(&client, &JobSpyClient::grabSuccess);
    QSignalSpy errorSpy(&client, &JobSpyClient::grabError);

    client.grabJob(QStringLiteral("https://www.linkedin.com/jobs/view/999999"));

    QVERIFY(successSpy.wait(5000));
    QCOMPARE(successSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 0);

    const auto record = successSpy.first().first().value<JobRecord>();
    QCOMPARE(record.id(), QStringLiteral("li-999999"));
    QCOMPARE(record.title(), QStringLiteral("Staff Security Engineer"));
    QCOMPARE(record.companyName(), QStringLiteral("Acme Corp"));
    QCOMPARE(record.isRemote(), true);

    qunsetenv("JOBSTARR_BRIDGE_PATH");
}

void TestScraperBoundary::testNonzeroExitStderr() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString mockScript = tempDir.path() + QStringLiteral("/jobspy_bridge.py");
    QFile file(mockScript);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(
        "#!/usr/bin/env python3\n"
        "import sys\n"
        "sys.stderr.write('Target job was removed or does not exist.\\n')\n"
        "sys.exit(3)\n"
    );
    file.close();
    QFile::setPermissions(mockScript, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    qputenv("JOBSTARR_BRIDGE_PATH", mockScript.toLocal8Bit());

    JobSpyClient client;
    QSignalSpy successSpy(&client, &JobSpyClient::grabSuccess);
    QSignalSpy errorSpy(&client, &JobSpyClient::grabError);

    client.grabJob(QStringLiteral("https://www.linkedin.com/jobs/view/000000"));

    QVERIFY(errorSpy.wait(5000));
    QCOMPARE(successSpy.count(), 0);
    QCOMPARE(errorSpy.count(), 1);

    const QString errMsg = errorSpy.first().first().toString();
    QVERIFY2(errMsg.contains(QStringLiteral("Target job was removed")), qPrintable(errMsg));

    qunsetenv("JOBSTARR_BRIDGE_PATH");
}

void TestScraperBoundary::testMalformedJson() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString mockScript = tempDir.path() + QStringLiteral("/jobspy_bridge.py");
    QFile file(mockScript);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(
        "#!/usr/bin/env python3\n"
        "import sys\n"
        "sys.stdout.write('Not a valid json response\\n')\n"
        "sys.exit(0)\n"
    );
    file.close();
    QFile::setPermissions(mockScript, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    qputenv("JOBSTARR_BRIDGE_PATH", mockScript.toLocal8Bit());

    JobSpyClient client;
    QSignalSpy errorSpy(&client, &JobSpyClient::grabError);

    client.grabJob(QStringLiteral("https://www.linkedin.com/jobs/view/123"));

    QVERIFY(errorSpy.wait(5000));
    QCOMPARE(errorSpy.count(), 1);
    const QString errMsg = errorSpy.first().first().toString();
    QVERIFY(errMsg.contains(QStringLiteral("Failed to parse")));

    qunsetenv("JOBSTARR_BRIDGE_PATH");
}

void TestScraperBoundary::testTimeout() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString mockScript = tempDir.path() + QStringLiteral("/jobspy_bridge.py");
    QFile file(mockScript);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(
        "#!/usr/bin/env python3\n"
        "import time, sys\n"
        "time.sleep(10)\n"
        "sys.exit(0)\n"
    );
    file.close();
    QFile::setPermissions(mockScript, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    qputenv("JOBSTARR_BRIDGE_PATH", mockScript.toLocal8Bit());

    JobSpyClient client;
    QSignalSpy errorSpy(&client, &JobSpyClient::grabError);

    // Set 200ms timeout
    client.grabJob(QStringLiteral("https://www.linkedin.com/jobs/view/123"), 200);

    QVERIFY(errorSpy.wait(3000));
    QCOMPARE(errorSpy.count(), 1);
    const QString errMsg = errorSpy.first().first().toString();
    QVERIFY(errMsg.contains(QStringLiteral("timed out")));

    qunsetenv("JOBSTARR_BRIDGE_PATH");
}

QTEST_MAIN(TestScraperBoundary)
#include "test_scraper_boundary.moc"
