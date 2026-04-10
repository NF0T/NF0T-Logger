#pragma once

#include <QList>
#include <QObject>
#include <QSqlQuery>
#include <QString>
#include <QStringList>

#include "core/logbook/Qso.h"

struct MigrateOptions {
    // Source — Log4OM MariaDB
    QString host         = QStringLiteral("localhost");
    int     port         = 3306;
    QString database     = QStringLiteral("log4om2");
    QString username;
    QString password;

    // Filters
    QString stationCallsign;  // empty = all stations
    int     limit  = 0;       // 0 = no limit (useful for dry-run testing)
    int     offset = 0;

    // Destination — NF0T Logger SQLite
    QString destSqlitePath;   // empty = use AppLocalDataLocation/log.db

    bool dryRun = false;      // read + map but do not write
};

struct MigrateResult {
    int read      = 0;
    int inserted  = 0;
    int skipped   = 0;   // duplicate constraint
    int errors    = 0;
    QStringList errorDetails;
};

/// Reads QSOs from a Log4OM v2 MariaDB database and inserts them into an
/// NF0T Logger SQLite database.
///
/// All work is synchronous and runs on the calling thread — run in a
/// QThread or worker if called from a GUI.
class Log4OmMigrator : public QObject
{
    Q_OBJECT

public:
    explicit Log4OmMigrator(QObject *parent = nullptr);

    MigrateResult migrate(const MigrateOptions &opts);

signals:
    /// Emitted periodically so callers can show progress.
    void progress(int done, int total);

private:
    static Qso rowToQso(const QSqlQuery &q);
    static void parseQsoConfirmations(const QString &json, Qso &qso);
};
