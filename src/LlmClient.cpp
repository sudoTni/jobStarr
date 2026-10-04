#include "LlmClient.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>

namespace jobstarr {

LlmClient::LlmClient(QNetworkAccessManager *networkManager, QObject *parent)
    : QObject(parent),
      m_timer(new QTimer(this)) {
    if (networkManager) {
        m_networkManager = networkManager;
        m_ownsNetworkManager = false;
    } else {
        m_networkManager = new QNetworkAccessManager(this);
        m_ownsNetworkManager = true;
    }

    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &LlmClient::onTimeout);
}

LlmClient::~LlmClient() {
    cancel();
}

bool LlmClient::isRunning() const {
    return m_currentReply != nullptr && m_currentReply->isRunning();
}

void LlmClient::cancel() {
    m_timer->stop();
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
}

void LlmClient::sendChatCompletion(const QString &endpoint,
                                    const QString &model,
                                    const QString &apiKey,
                                    const QString &systemPrompt,
                                    const QString &userPrompt,
                                    int timeoutMs,
                                    const QString &reasoningEffort) {
    if (isRunning()) {
        const QString err = QStringLiteral("An LLM operation is already in progress.");
        emit chatError(err);
        if (m_isJudging) emit judgeError(err);
        return;
    }

    const QString trimmedEndpoint = endpoint.trimmed();
    if (trimmedEndpoint.isEmpty()) {
        const QString err = QStringLiteral("OpenAI API endpoint is not configured.");
        emit chatError(err);
        if (m_isJudging) emit judgeError(err);
        return;
    }

    const QUrl url(trimmedEndpoint);
    if (!url.isValid() || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))) {
        const QString err = QStringLiteral("Invalid API endpoint URL. Must start with http:// or https://.");
        emit chatError(err);
        if (m_isJudging) emit judgeError(err);
        return;
    }

    if (model.trimmed().isEmpty()) {
        const QString err = QStringLiteral("LLM Model is not configured.");
        emit chatError(err);
        if (m_isJudging) emit judgeError(err);
        return;
    }

    // Build chat completions request payload
    QJsonObject rootObj;
    rootObj.insert(QLatin1String("model"), model.trimmed());

    QJsonArray messagesArr;

    if (!systemPrompt.trimmed().isEmpty()) {
        QJsonObject sysMsg;
        sysMsg.insert(QLatin1String("role"), QLatin1String("system"));
        sysMsg.insert(QLatin1String("content"), systemPrompt);
        messagesArr.append(sysMsg);
    }

    QJsonObject userMsg;
    userMsg.insert(QLatin1String("role"), QLatin1String("user"));
    userMsg.insert(QLatin1String("content"), userPrompt);
    messagesArr.append(userMsg);

    rootObj.insert(QLatin1String("messages"), messagesArr);

    if (!reasoningEffort.trimmed().isEmpty()) {
        rootObj.insert(QLatin1String("reasoning_effort"), reasoningEffort.trimmed());
    }

    const QByteArray requestBody = QJsonDocument(rootObj).toJson(QJsonDocument::Compact);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("HTTP-Referer", "https://github.com/sudoTni/jobStarr");
    request.setRawHeader("X-Title", "jobStarr");
    if (!apiKey.trimmed().isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + apiKey.trimmed().toUtf8());
    }

    emit chatStarted();
    if (m_isJudging) {
        emit judgeStarted();
    }

    m_timedOut = false;
    m_timer->start(timeoutMs);

    m_currentReply = m_networkManager->post(request, requestBody);
    connect(m_currentReply, &QNetworkReply::finished, this, &LlmClient::onReplyFinished);
}

void LlmClient::judgeJob(const QString &endpoint,
                         const QString &model,
                         const QString &apiKey,
                         const QString &systemPrompt,
                         const QString &userPrompt,
                         int timeoutMs,
                         const QString &reasoningEffort) {
    m_isJudging = true;
    sendChatCompletion(endpoint, model, apiKey, systemPrompt, userPrompt, timeoutMs, reasoningEffort);
}

void LlmClient::onTimeout() {
    if (isRunning()) {
        m_timedOut = true;
        const bool wasJudging = m_isJudging;
        m_currentReply->abort();
        const QString err = QStringLiteral("LLM API request timed out.");
        emit chatError(err);
        if (wasJudging) {
            emit judgeError(err);
        }
    }
}

void LlmClient::onReplyFinished() {
    m_timer->stop();
    if (!m_currentReply) return;

    const bool wasJudging = m_isJudging;
    m_isJudging = false;

    QNetworkReply *reply = m_currentReply;
    m_currentReply = nullptr;
    reply->deleteLater();

    if (m_timedOut) {
        return;
    }

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netError = reply->error();
    const QByteArray responseData = reply->readAll();

    if (netError != QNetworkReply::NoError) {
        QString err;
        if (statusCode == 401 || statusCode == 403) {
            err = QStringLiteral("Authentication failed (HTTP %1). Please verify your API key.").arg(statusCode);
        } else if (statusCode > 0) {
            err = QStringLiteral("LLM API returned HTTP error %1: %2").arg(statusCode).arg(reply->errorString());
        } else {
            err = QStringLiteral("Network error contacting LLM endpoint: %1").arg(reply->errorString());
        }

        emit chatError(err);
        if (wasJudging) {
            emit judgeError(err);
        }
        return;
    }

    // Parse Chat Completions JSON response
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        const QString err = QStringLiteral("Malformed Chat Completions JSON response: %1").arg(parseError.errorString());
        emit chatError(err);
        if (wasJudging) emit judgeError(err);
        return;
    }

    const QJsonObject respObj = doc.object();
    if (!respObj.contains(QLatin1String("choices")) || !respObj.value(QLatin1String("choices")).isArray()) {
        const QString err = QStringLiteral("Malformed API response: missing 'choices' array.");
        emit chatError(err);
        if (wasJudging) emit judgeError(err);
        return;
    }

    const QJsonArray choices = respObj.value(QLatin1String("choices")).toArray();
    if (choices.isEmpty() || !choices.at(0).isObject()) {
        const QString err = QStringLiteral("Malformed API response: 'choices' array is empty or contains non-objects.");
        emit chatError(err);
        if (wasJudging) emit judgeError(err);
        return;
    }

    const QJsonObject firstChoice = choices.at(0).toObject();
    if (!firstChoice.contains(QLatin1String("message")) || !firstChoice.value(QLatin1String("message")).isObject()) {
        const QString err = QStringLiteral("Malformed API response: missing 'message' object in choice.");
        emit chatError(err);
        if (wasJudging) emit judgeError(err);
        return;
    }

    const QJsonObject messageObj = firstChoice.value(QLatin1String("message")).toObject();
    if (!messageObj.contains(QLatin1String("content"))) {
        const QString err = QStringLiteral("Malformed API response: missing 'content' in message.");
        emit chatError(err);
        if (wasJudging) emit judgeError(err);
        return;
    }

    const QString content = messageObj.value(QLatin1String("content")).toString();

    emit chatSuccess(content);

    if (wasJudging) {
        QString resultParseError;
        auto judgeOpt = JobJudgeResult::parseLlmResponse(content, &resultParseError);
        if (!judgeOpt.has_value()) {
            emit judgeError(QStringLiteral("Failed to parse Job Judge result: %1").arg(resultParseError));
            return;
        }
        emit judgeSuccess(judgeOpt.value());
    }
}

} // namespace jobstarr
