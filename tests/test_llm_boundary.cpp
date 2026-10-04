#include <QTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include "LlmClient.h"

using namespace jobstarr;

class MockHttpServer : public QTcpServer {
    Q_OBJECT
public:
    int statusCode{200};
    QByteArray responseBody;
    int delayMs{0};
    QByteArray lastReceivedAuthHeader;
    QByteArray lastReceivedRequestBody;
    QByteArray allReceivedData;

    explicit MockHttpServer(QObject *parent = nullptr) : QTcpServer(parent) {
        connect(this, &QTcpServer::newConnection, this, &MockHttpServer::onNewConnection);
    }

private slots:
    void onNewConnection() {
        QTcpSocket *socket = nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, [this, socket]() {
            const QByteArray data = socket->readAll();
            allReceivedData = data;
            // Extract Authorization header and body
            const int authIdx = data.indexOf("Authorization: ");
            if (authIdx != -1) {
                const int endLine = data.indexOf("\r\n", authIdx);
                lastReceivedAuthHeader = data.mid(authIdx, endLine - authIdx);
            }
            const int bodyIdx = data.indexOf("\r\n\r\n");
            if (bodyIdx != -1) {
                lastReceivedRequestBody = data.mid(bodyIdx + 4);
            }

            auto sendResponse = [this, socket]() {
                QByteArray resp;
                resp.append(QString("HTTP/1.1 %1 OK\r\n").arg(statusCode).toUtf8());
                resp.append("Content-Type: application/json\r\n");
                resp.append(QString("Content-Length: %1\r\n\r\n").arg(responseBody.size()).toUtf8());
                resp.append(responseBody);
                socket->write(resp);
                socket->flush();
                socket->disconnectFromHost();
            };

            if (delayMs > 0) {
                QTimer::singleShot(delayMs, sendResponse);
            } else {
                sendResponse();
            }
        });
    }
};

class TestLlmBoundary : public QObject {
    Q_OBJECT

private slots:
    void testSuccessfulChatCompletion();
    void test401AuthenticationFailure();
    void test500InternalServerError();
    void testTimeout();
    void testMalformedApiJson();
    void testMissingChoicesOrContent();
    void testApiSuccessWithMalformedJudgeJson();
    void testReasoningEffortInPayload();
};

void TestLlmBoundary::testSuccessfulChatCompletion() {
    MockHttpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));

    server.statusCode = 200;
    server.responseBody = QByteArray(R"({
      "choices": [
        {
          "message": {
            "role": "assistant",
            "content": "[{\"jobTitle\":\"Cyber Analyst\",\"isVeryHighlyAligned\":true,\"rationale\":\"Matches experience well. Recommend applying.\",\"confidence\":0.92}]"
          }
        }
      ]
    })");

    LlmClient client;
    QSignalSpy successSpy(&client, &LlmClient::judgeSuccess);
    QSignalSpy errorSpy(&client, &LlmClient::judgeError);

    const QString endpoint = QString("http://127.0.0.1:%1/v1/chat/completions").arg(server.serverPort());
    client.judgeJob(endpoint, QStringLiteral("gpt-4o"), QStringLiteral("test-key-123"),
                    QStringLiteral("Sys prompt"), QStringLiteral("User prompt"));

    QVERIFY(successSpy.wait(5000));
    QCOMPARE(successSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 0);

    const auto res = successSpy.first().first().value<JobJudgeResult>();
    QCOMPARE(res.jobTitle, QStringLiteral("Cyber Analyst"));
    QCOMPARE(res.isVeryHighlyAligned, true);
    QCOMPARE(res.confidence, 0.92);

    QVERIFY(server.lastReceivedAuthHeader.contains("Bearer test-key-123"));
    QVERIFY(server.allReceivedData.toLower().contains("http-referer: https://github.com/sudotni/jobstarr"));
    QVERIFY(server.allReceivedData.contains("X-Title: jobStarr"));
}

void TestLlmBoundary::test401AuthenticationFailure() {
    MockHttpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));

    server.statusCode = 401;
    server.responseBody = QByteArray(R"({"error": {"message": "Invalid API key"}})");

    LlmClient client;
    QSignalSpy errorSpy(&client, &LlmClient::judgeError);

    const QString endpoint = QString("http://127.0.0.1:%1/v1/chat/completions").arg(server.serverPort());
    client.judgeJob(endpoint, QStringLiteral("gpt-4o"), QStringLiteral("bad-key"),
                    QStringLiteral("sys"), QStringLiteral("user"));

    QVERIFY(errorSpy.wait(5000));
    QCOMPARE(errorSpy.count(), 1);
    const QString errMsg = errorSpy.first().first().toString();
    QVERIFY(errMsg.contains(QStringLiteral("Authentication failed (HTTP 401)")));
}

void TestLlmBoundary::test500InternalServerError() {
    MockHttpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));

    server.statusCode = 500;
    server.responseBody = QByteArray(R"({"error": {"message": "Internal error"}})");

    LlmClient client;
    QSignalSpy errorSpy(&client, &LlmClient::judgeError);

    const QString endpoint = QString("http://127.0.0.1:%1/v1/chat/completions").arg(server.serverPort());
    client.judgeJob(endpoint, QStringLiteral("gpt-4o"), QStringLiteral("key"),
                    QStringLiteral("sys"), QStringLiteral("user"));

    QVERIFY(errorSpy.wait(5000));
    QCOMPARE(errorSpy.count(), 1);
    const QString errMsg = errorSpy.first().first().toString();
    QVERIFY(errMsg.contains(QStringLiteral("HTTP error 500")));
}

void TestLlmBoundary::testTimeout() {
    MockHttpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));

    server.statusCode = 200;
    server.delayMs = 2000;
    server.responseBody = QByteArray("{}");

    LlmClient client;
    QSignalSpy errorSpy(&client, &LlmClient::judgeError);

    const QString endpoint = QString("http://127.0.0.1:%1/v1/chat/completions").arg(server.serverPort());
    // 200ms timeout
    client.judgeJob(endpoint, QStringLiteral("gpt-4o"), QStringLiteral("key"),
                    QStringLiteral("sys"), QStringLiteral("user"), 200);

    QVERIFY(errorSpy.wait(3000));
    QCOMPARE(errorSpy.count(), 1);
    const QString errMsg = errorSpy.first().first().toString();
    QVERIFY(errMsg.contains(QStringLiteral("timed out")));
}

void TestLlmBoundary::testMalformedApiJson() {
    MockHttpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));

    server.statusCode = 200;
    server.responseBody = QByteArray("<html><body>502 Bad Gateway</body></html>");

    LlmClient client;
    QSignalSpy errorSpy(&client, &LlmClient::judgeError);

    const QString endpoint = QString("http://127.0.0.1:%1/v1/chat/completions").arg(server.serverPort());
    client.judgeJob(endpoint, QStringLiteral("gpt-4o"), QStringLiteral("key"),
                    QStringLiteral("sys"), QStringLiteral("user"));

    QVERIFY(errorSpy.wait(5000));
    QCOMPARE(errorSpy.count(), 1);
    const QString errMsg = errorSpy.first().first().toString();
    QVERIFY(errMsg.contains(QStringLiteral("Malformed Chat Completions JSON response")));
}

void TestLlmBoundary::testMissingChoicesOrContent() {
    MockHttpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));

    server.statusCode = 200;
    server.responseBody = QByteArray(R"({"id":"chatcmpl-123","choices":[]})");

    LlmClient client;
    QSignalSpy errorSpy(&client, &LlmClient::judgeError);

    const QString endpoint = QString("http://127.0.0.1:%1/v1/chat/completions").arg(server.serverPort());
    client.judgeJob(endpoint, QStringLiteral("gpt-4o"), QStringLiteral("key"),
                    QStringLiteral("sys"), QStringLiteral("user"));

    QVERIFY(errorSpy.wait(5000));
    QCOMPARE(errorSpy.count(), 1);
    const QString errMsg = errorSpy.first().first().toString();
    QVERIFY(errMsg.contains(QStringLiteral("choices")));
}

void TestLlmBoundary::testApiSuccessWithMalformedJudgeJson() {
    MockHttpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));

    server.statusCode = 200;
    server.responseBody = QByteArray(R"({
      "choices": [
        {
          "message": {
            "role": "assistant",
            "content": "I am not able to output JSON: candidate looks good."
          }
        }
      ]
    })");

    LlmClient client;
    QSignalSpy errorSpy(&client, &LlmClient::judgeError);

    const QString endpoint = QString("http://127.0.0.1:%1/v1/chat/completions").arg(server.serverPort());
    client.judgeJob(endpoint, QStringLiteral("gpt-4o"), QStringLiteral("key"),
                    QStringLiteral("sys"), QStringLiteral("user"));

    QVERIFY(errorSpy.wait(5000));
    QCOMPARE(errorSpy.count(), 1);
    const QString errMsg = errorSpy.first().first().toString();
    QVERIFY(errMsg.contains(QStringLiteral("Failed to parse Job Judge result")));
}

void TestLlmBoundary::testReasoningEffortInPayload() {
    MockHttpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));

    server.statusCode = 200;
    server.responseBody = QByteArray(R"({
      "choices": [
        {
          "message": {
            "role": "assistant",
            "content": "ok"
          }
        }
      ]
    })");

    LlmClient client;
    QSignalSpy successSpy(&client, &LlmClient::chatSuccess);

    const QString endpoint = QString("http://127.0.0.1:%1/v1/chat/completions").arg(server.serverPort());

    // 1. With reasoning_effort = "high"
    client.sendChatCompletion(endpoint, QStringLiteral("openai/gpt-5.4"), QStringLiteral("test-key"),
                             QStringLiteral("sys"), QStringLiteral("user"), 5000, QStringLiteral("high"));

    QVERIFY(successSpy.wait(5000));
    QCOMPARE(successSpy.count(), 1);

    QJsonDocument docWithEffort = QJsonDocument::fromJson(server.lastReceivedRequestBody);
    QVERIFY(docWithEffort.isObject());
    QCOMPARE(docWithEffort.object().value(QLatin1String("reasoning_effort")).toString(), QStringLiteral("high"));

    // 2. Without reasoning_effort (empty string)
    successSpy.clear();
    client.sendChatCompletion(endpoint, QStringLiteral("openai/gpt-5.4"), QStringLiteral("test-key"),
                             QStringLiteral("sys"), QStringLiteral("user"), 5000, QString());

    QVERIFY(successSpy.wait(5000));
    QCOMPARE(successSpy.count(), 1);

    QJsonDocument docWithoutEffort = QJsonDocument::fromJson(server.lastReceivedRequestBody);
    QVERIFY(docWithoutEffort.isObject());
    QVERIFY(!docWithoutEffort.object().contains(QLatin1String("reasoning_effort")));
}

QTEST_MAIN(TestLlmBoundary)
#include "test_llm_boundary.moc"
