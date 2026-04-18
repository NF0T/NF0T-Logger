// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <atomic>
#include <expected>
#include <QSqlDatabase>
#include <QString>

#include "DatabaseInterface.h"

/// Shared SQL implementation for both SQLite and MariaDB backends.
/// Subclasses implement open() (connection setup) and initSchema() (DDL).
/// All CRUD operations are identical across backends — Qt's SQL layer
/// abstracts the driver differences.
class SqlBackendBase : public DatabaseInterface
{
public:
    SqlBackendBase();
    ~SqlBackendBase() override;

    void close()  override;
    bool isOpen() const override;

    std::expected<qint64, QString> insertQso(Qso &qso)           override;
    std::expected<void,   QString> updateQso(const Qso &qso)     override;
    std::expected<void,   QString> deleteQso(qint64 id)          override;
    std::expected<QList<Qso>, QString> fetchQsos(const QsoFilter &filter = {}) override;
    std::expected<int,        QString> qsoCount()                override;

protected:
    QSqlDatabase m_db;
    QString      m_connectionName;

    std::expected<void, QString> execQuery(const QString &sql);

private:
    static void bindQso(QSqlQuery &q, const Qso &qso);
    static Qso  qsoFromQuery(const QSqlQuery &q);

    static const char *INSERT_SQL;
    static const char *UPDATE_SQL;

    static std::atomic<int> s_connectionCounter;
};
