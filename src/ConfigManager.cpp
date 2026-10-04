#include "ConfigManager.h"
#include "ProjectPaths.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <yaml-cpp/yaml.h>
#include <sys/stat.h>

inline void initJobStarrResources() {
    Q_INIT_RESOURCE(jobstarr);
}

namespace jobstarr {

static QString readResourceFile(const QString &resourcePath) {
    initJobStarrResources();
    QFile file(resourcePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString::fromUtf8(file.readAll());
    }
    return QString();
}

ConfigManager::ConfigManager(const QString &customConfigPath)
    : m_customConfigPath(customConfigPath) {}

QString ConfigManager::configFilePath() const {
    if (!m_customConfigPath.isEmpty()) {
        return m_customConfigPath;
    }
    return QCoreApplication::applicationDirPath() + QLatin1String("/jobStarr.yaml");
}

void ConfigManager::setCustomConfigPath(const QString &path) {
    m_customConfigPath = path;
}

QString ConfigManager::loadDefaultSystemPrompt() {
    return readResourceFile(QStringLiteral(":/sysprompts/veritas_sys_prompt.md"));
}

QString ConfigManager::loadDefaultJobJudgePrompt() {
    return readResourceFile(QStringLiteral(":/prompts/jj_prompt.md"));
}

QString ConfigManager::loadDefaultMakeMaterialsPrompt() {
    return readResourceFile(QStringLiteral(":/prompts/mm_prompt.md"));
}

QString ConfigManager::loadDefaultResume() {
    return readResourceFile(QStringLiteral(":/candidate_data/my_resume.md"));
}

QString ConfigManager::loadDefaultProfessionalTitle() {
    return readResourceFile(QStringLiteral(":/candidate_data/my_professional_title.md")).trimmed();
}

QString ConfigManager::loadDefaultProfessionalSummary() {
    return readResourceFile(QStringLiteral(":/candidate_data/my_professional_summary.md"));
}

QString ConfigManager::loadDefaultKeySkills() {
    return readResourceFile(QStringLiteral(":/candidate_data/my_key_skills.md"));
}

QString ConfigManager::loadDefaultTestimonials() {
    return readResourceFile(QStringLiteral(":/candidate_data/my_testimonials.md"));
}

bool ConfigManager::load(AppConfig &config, QString *errorMessage) {
    const QString path = configFilePath();
    const QFileInfo fileInfo(path);

    if (!fileInfo.exists()) {
        // File does not exist: initialize using defaults
        config.version = QStringLiteral("0.2.0");
        config.apiEndpoint = QStringLiteral("https://api.openai.com/v1/chat/completions");
        config.model = QStringLiteral("gpt-4o");
        config.apiKey = QString();
        config.reasoningEffort = QString();
        config.timeoutSeconds = 300;

        config.searchEndpoint = QString();
        config.searchModel = QString();
        config.searchApiKey = QString();
        config.searchReasoningEffort = QString();
        config.searchTimeoutSeconds = 300;

        config.systemPrompt = loadDefaultSystemPrompt();
        config.jobJudgePrompt = loadDefaultJobJudgePrompt();
        config.makeMaterialsPrompt = loadDefaultMakeMaterialsPrompt();

        config.professionalTitle = loadDefaultProfessionalTitle();
        config.professionalSummary = loadDefaultProfessionalSummary();
        config.keySkills = loadDefaultKeySkills();
        config.myResume = loadDefaultResume();
        config.testimonials = loadDefaultTestimonials();

        config.resumeTemplatePath = ProjectPaths::defaultResumeTemplatePath();
        config.coverLetterTemplatePath = ProjectPaths::defaultCoverLetterTemplatePath();
        return true;
    }

    try {
        YAML::Node doc = YAML::LoadFile(path.toStdString());

        if (doc["version"]) {
            config.version = QString::fromStdString(doc["version"].as<std::string>());
        } else {
            config.version = QStringLiteral("0.1.0");
        }

        if (doc["api"]) {
            YAML::Node apiNode = doc["api"];

            // Check if nested primary api exists (v0.2.0) or flat api (v0.1.0)
            if (apiNode["primary"]) {
                YAML::Node prim = apiNode["primary"];
                if (prim["endpoint"]) config.apiEndpoint = QString::fromStdString(prim["endpoint"].as<std::string>());
                if (prim["model"]) config.model = QString::fromStdString(prim["model"].as<std::string>());
                if (prim["api_key"]) config.apiKey = QString::fromStdString(prim["api_key"].as<std::string>());
                if (prim["reasoning_effort"]) config.reasoningEffort = QString::fromStdString(prim["reasoning_effort"].as<std::string>()).trimmed();
                else config.reasoningEffort.clear();
                if (prim["timeout_seconds"]) config.timeoutSeconds = prim["timeout_seconds"].as<int>();
                else config.timeoutSeconds = 300;
            } else {
                // v0.1.0 flat schema
                if (apiNode["endpoint"]) config.apiEndpoint = QString::fromStdString(apiNode["endpoint"].as<std::string>());
                if (apiNode["model"]) config.model = QString::fromStdString(apiNode["model"].as<std::string>());
                if (apiNode["api_key"]) config.apiKey = QString::fromStdString(apiNode["api_key"].as<std::string>());
                if (apiNode["reasoning_effort"]) config.reasoningEffort = QString::fromStdString(apiNode["reasoning_effort"].as<std::string>()).trimmed();
                else config.reasoningEffort.clear();
                if (apiNode["timeout_seconds"]) config.timeoutSeconds = apiNode["timeout_seconds"].as<int>();
                else if (apiNode["timeout"]) config.timeoutSeconds = apiNode["timeout"].as<int>();
                else config.timeoutSeconds = 300;
            }

            if (apiNode["search"]) {
                YAML::Node searchNode = apiNode["search"];
                if (searchNode["endpoint"]) config.searchEndpoint = QString::fromStdString(searchNode["endpoint"].as<std::string>());
                if (searchNode["model"]) config.searchModel = QString::fromStdString(searchNode["model"].as<std::string>());
                if (searchNode["api_key"]) config.searchApiKey = QString::fromStdString(searchNode["api_key"].as<std::string>());
                if (searchNode["reasoning_effort"]) config.searchReasoningEffort = QString::fromStdString(searchNode["reasoning_effort"].as<std::string>()).trimmed();
                else config.searchReasoningEffort.clear();
                if (searchNode["timeout_seconds"]) config.searchTimeoutSeconds = searchNode["timeout_seconds"].as<int>();
                else config.searchTimeoutSeconds = 300;
            } else {
                config.searchEndpoint.clear();
                config.searchModel.clear();
                config.searchApiKey.clear();
                config.searchReasoningEffort.clear();
                config.searchTimeoutSeconds = 300;
            }
        }

        if (doc["prompts"]) {
            YAML::Node promptsNode = doc["prompts"];
            if (promptsNode["system"]) {
                config.systemPrompt = QString::fromStdString(promptsNode["system"].as<std::string>());
            } else {
                config.systemPrompt = loadDefaultSystemPrompt();
            }
            if (promptsNode["job_judge"]) {
                config.jobJudgePrompt = QString::fromStdString(promptsNode["job_judge"].as<std::string>());
            } else {
                config.jobJudgePrompt = loadDefaultJobJudgePrompt();
            }
            if (promptsNode["make_materials"]) {
                config.makeMaterialsPrompt = QString::fromStdString(promptsNode["make_materials"].as<std::string>());
            } else {
                config.makeMaterialsPrompt = loadDefaultMakeMaterialsPrompt();
            }
        } else {
            config.systemPrompt = loadDefaultSystemPrompt();
            config.jobJudgePrompt = loadDefaultJobJudgePrompt();
            config.makeMaterialsPrompt = loadDefaultMakeMaterialsPrompt();
        }

        if (doc["candidate"]) {
            YAML::Node candidateNode = doc["candidate"];
            if (candidateNode["professional_title"]) {
                config.professionalTitle = QString::fromStdString(candidateNode["professional_title"].as<std::string>()).trimmed();
            } else {
                config.professionalTitle = loadDefaultProfessionalTitle();
            }
            if (candidateNode["professional_summary"]) {
                config.professionalSummary = QString::fromStdString(candidateNode["professional_summary"].as<std::string>());
            } else {
                config.professionalSummary = loadDefaultProfessionalSummary();
            }
            if (candidateNode["key_skills"]) {
                config.keySkills = QString::fromStdString(candidateNode["key_skills"].as<std::string>());
            } else {
                config.keySkills = loadDefaultKeySkills();
            }
            if (candidateNode["resume"]) {
                config.myResume = QString::fromStdString(candidateNode["resume"].as<std::string>());
            } else {
                config.myResume = loadDefaultResume();
            }
            if (candidateNode["testimonials"]) {
                config.testimonials = QString::fromStdString(candidateNode["testimonials"].as<std::string>());
            } else {
                config.testimonials = loadDefaultTestimonials();
            }
        } else {
            config.professionalTitle = loadDefaultProfessionalTitle();
            config.professionalSummary = loadDefaultProfessionalSummary();
            config.keySkills = loadDefaultKeySkills();
            config.myResume = loadDefaultResume();
            config.testimonials = loadDefaultTestimonials();
        }

        if (doc["materials"]) {
            YAML::Node matNode = doc["materials"];
            if (matNode["resume_template"]) {
                config.resumeTemplatePath = QString::fromStdString(matNode["resume_template"].as<std::string>());
            } else {
                config.resumeTemplatePath = ProjectPaths::defaultResumeTemplatePath();
            }
            if (matNode["cover_letter_template"]) {
                config.coverLetterTemplatePath = QString::fromStdString(matNode["cover_letter_template"].as<std::string>());
            } else {
                config.coverLetterTemplatePath = ProjectPaths::defaultCoverLetterTemplatePath();
            }
        } else {
            config.resumeTemplatePath = ProjectPaths::defaultResumeTemplatePath();
            config.coverLetterTemplatePath = ProjectPaths::defaultCoverLetterTemplatePath();
        }

        return true;
    } catch (const std::exception &ex) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to parse YAML configuration: %1").arg(ex.what());
        }
        return false;
    }
}

bool ConfigManager::save(const AppConfig &config, QString *errorMessage) {
    const QString path = configFilePath();

    try {
        YAML::Emitter emitter;
        emitter << YAML::BeginMap;

        // Upgrade/save as version 0.2.0 unless explicitly specified otherwise
        const QString ver = config.version.isEmpty() ? QStringLiteral("0.2.0") : config.version;
        emitter << YAML::Key << "version" << YAML::Value << ver.toStdString();

        emitter << YAML::Key << "api" << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "primary" << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "endpoint" << YAML::Value << config.apiEndpoint.toStdString();
        emitter << YAML::Key << "model" << YAML::Value << config.model.toStdString();
        emitter << YAML::Key << "api_key" << YAML::Value << config.apiKey.toStdString();
        emitter << YAML::Key << "reasoning_effort" << YAML::Value << config.reasoningEffort.toStdString();
        emitter << YAML::Key << "timeout_seconds" << YAML::Value << config.timeoutSeconds;
        emitter << YAML::EndMap;

        emitter << YAML::Key << "search" << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "endpoint" << YAML::Value << config.searchEndpoint.toStdString();
        emitter << YAML::Key << "model" << YAML::Value << config.searchModel.toStdString();
        emitter << YAML::Key << "api_key" << YAML::Value << config.searchApiKey.toStdString();
        emitter << YAML::Key << "reasoning_effort" << YAML::Value << config.searchReasoningEffort.toStdString();
        emitter << YAML::Key << "timeout_seconds" << YAML::Value << config.searchTimeoutSeconds;
        emitter << YAML::EndMap;
        emitter << YAML::EndMap;

        emitter << YAML::Key << "prompts" << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "system" << YAML::Value << YAML::Literal << config.systemPrompt.toStdString();
        emitter << YAML::Key << "job_judge" << YAML::Value << YAML::Literal << config.jobJudgePrompt.toStdString();
        emitter << YAML::Key << "make_materials" << YAML::Value << YAML::Literal << config.makeMaterialsPrompt.toStdString();
        emitter << YAML::EndMap;

        emitter << YAML::Key << "candidate" << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "professional_title" << YAML::Value << config.professionalTitle.toStdString();
        emitter << YAML::Key << "professional_summary" << YAML::Value << YAML::Literal << config.professionalSummary.toStdString();
        emitter << YAML::Key << "key_skills" << YAML::Value << YAML::Literal << config.keySkills.toStdString();
        emitter << YAML::Key << "resume" << YAML::Value << YAML::Literal << config.myResume.toStdString();
        emitter << YAML::Key << "testimonials" << YAML::Value << YAML::Literal << config.testimonials.toStdString();
        emitter << YAML::EndMap;

        emitter << YAML::Key << "materials" << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "resume_template" << YAML::Value << config.resumeTemplatePath.toStdString();
        emitter << YAML::Key << "cover_letter_template" << YAML::Value << config.coverLetterTemplatePath.toStdString();
        emitter << YAML::EndMap;

        emitter << YAML::EndMap;

        if (!emitter.good()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("YAML emission failed: %1").arg(QString::fromStdString(emitter.GetLastError()));
            }
            return false;
        }

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Failed to open '%1' for writing: %2")
                                    .arg(path, file.errorString());
            }
            return false;
        }

        const QByteArray bytes = QByteArray(emitter.c_str(), emitter.size()) + "\n";
        if (file.write(bytes) != bytes.size()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Failed to write all configuration bytes to '%1'.").arg(path);
            }
            file.close();
            return false;
        }
        file.close();

        // Enforce user-only permissions (0600) on Linux
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
        chmod(path.toLocal8Bit().constData(), S_IRUSR | S_IWUSR);

        return true;
    } catch (const std::exception &ex) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Exception while saving configuration: %1").arg(ex.what());
        }
        return false;
    }
}

} // namespace jobstarr
