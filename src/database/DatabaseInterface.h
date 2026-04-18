// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <expected>
#include <QList>
#include <QString>
#include <QVariantMap>

#include "core/logbook/Qso.h"
#include "core/logbook/QsoFilter.h"

/// Pure abstract interface for all database backends.
/// All operations return std::expected<T, QString>; the error string
/// describes the failure.  Business logic should depend only on this
/// type, never on a concrete backend.
class DatabaseInterface
{
public:
    virtual ~DatabaseInterface() = default;

    // Connection lifecycle
    virtual std::expected<void, QString> open(const QVariantMap &config) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    /// Create/migrate schema to current version. Call once after open().
    virtual std::expected<void, QString> initSchema() = 0;

    // CRUD
    /// Insert a new QSO. On success, returns the new row id.
    virtual std::expected<qint64, QString> insertQso(Qso &qso) = 0;
    virtual std::expected<void,   QString> updateQso(const Qso &qso) = 0;
    virtual std::expected<void,   QString> deleteQso(qint64 id) = 0;

    // Queries
    virtual std::expected<QList<Qso>, QString> fetchQsos(const QsoFilter &filter = {}) = 0;
    virtual std::expected<int,        QString> qsoCount() = 0;
};
