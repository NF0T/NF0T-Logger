#pragma once

#include <optional>
#include <QString>
#include <QDateTime>

/// Criteria for filtering QSO queries from the database.
struct QsoFilter
{
    std::optional<QString>   call;         // substring match
    std::optional<QString>   band;         // exact match
    std::optional<QString>   mode;         // exact match
    std::optional<QDateTime> from;         // datetime_on >= from (UTC)
    std::optional<QDateTime> to;           // datetime_on <= to (UTC)
    std::optional<bool>      lotwPending;    // sent Y but rcvd N
    std::optional<bool>      eqslPending;
    std::optional<bool>      qrzPending;
    std::optional<bool>      clublogPending;

    std::optional<bool>      lotwUnsent;     // sent != Y (not yet uploaded)
    std::optional<bool>      eqslUnsent;
    std::optional<bool>      qrzUnsent;
    std::optional<bool>      clublogUnsent;

    QString orderBy   = QStringLiteral("datetime_on");
    bool    ascending = false;
    int     limit     = 0;    // 0 = no limit
    int     offset    = 0;
};
