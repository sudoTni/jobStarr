#pragma once

#include <QString>

namespace jobstarr {

enum class ScrapeErrorKind {
    NoError,
    NetworkError,
    HttpError,
    Timeout,
    RateLimited,
    JobNotFound,
    AuthWall,
    ParseError,
    UnsupportedHost,
    MalformedUrl
};

struct ScrapeError {
    ScrapeErrorKind kind{ScrapeErrorKind::NoError};
    QString message;
    int httpStatusCode{0};

    bool isError() const { return kind != ScrapeErrorKind::NoError; }
};

} // namespace jobstarr
