#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>

#include "core/logbook/Qso.h"
#include "core/logbook/QsoFilter.h"

/// Pure abstract interface for all database backends.
/// Business logic should depend only on this type, never on a concrete backend.
class DatabaseInterface
{
public:
    virtual ~DatabaseInterface() = default;

    // Connection lifecycle
    virtual bool open(const QVariantMap &config) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    /// Create/migrate schema to current version. Call once after open().
    virtual bool initSchema() = 0;

    // CRUD
    /// Insert a new QSO. On success, sets qso.id to the new row id. Returns false on error.
    virtual bool insertQso(Qso &qso) = 0;
    virtual bool updateQso(const Qso &qso) = 0;
    virtual bool deleteQso(qint64 id) = 0;

    // Queries
    virtual QList<Qso> fetchQsos(const QsoFilter &filter = {}) = 0;
    virtual int        qsoCount() = 0;

    virtual QString lastError() const = 0;
};
