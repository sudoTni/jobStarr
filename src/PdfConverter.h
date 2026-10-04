#pragma once

#include <QString>
#include <QStringList>
#include <memory>
#include <functional>

namespace jobstarr {

class IPdfProcessRunner {
public:
    virtual ~IPdfProcessRunner() = default;
    virtual bool run(const QString &program,
                     const QStringList &arguments,
                     int timeoutMs,
                     int *exitCode,
                     QString *stdErr) = 0;
};

class DefaultPdfProcessRunner : public IPdfProcessRunner {
public:
    bool run(const QString &program,
             const QStringList &arguments,
             int timeoutMs,
             int *exitCode,
             QString *stdErr) override;
};

class PdfConverter {
public:
    explicit PdfConverter(std::shared_ptr<IPdfProcessRunner> runner = nullptr);

    static QString findConverterExecutable();

    bool convertToPdf(const QString &odtFilePath,
                      const QString &outputDir,
                      QString *resultingPdfPath = nullptr,
                      QString *errorMessage = nullptr,
                      int timeoutMs = 60000);

private:
    std::shared_ptr<IPdfProcessRunner> m_runner;
};

} // namespace jobstarr
