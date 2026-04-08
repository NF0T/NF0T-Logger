#pragma once

#include <atomic>
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

    void    close()   override;
    bool    isOpen()  const override;
    QString lastError() const override;

    bool insertQso(Qso &qso)           override;
    bool updateQso(const Qso &qso)     override;
    bool deleteQso(qint64 id)          override;
    QList<Qso> fetchQsos(const QsoFilter &filter = {}) override;
    int        qsoCount()              override;

protected:
    QSqlDatabase m_db;
    QString      m_connectionName;
    QString      m_lastError;

    bool execQuery(const QString &sql);

private:
    static void bindQso(QSqlQuery &q, const Qso &qso);
    static Qso  qsoFromQuery(const QSqlQuery &q);

    static const char *INSERT_SQL;
    static const char *UPDATE_SQL;

    static std::atomic<int> s_connectionCounter;
};
