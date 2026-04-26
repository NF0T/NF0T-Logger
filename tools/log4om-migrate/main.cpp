// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

#include "Log4OmMigrator.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("log4om-migrate"));
    app.setApplicationVersion(QStringLiteral(APP_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Migrate contacts from a Log4OM v2 MariaDB database into an\n"
                        "NF0T Logger SQLite database.\n\n"
                        "Example:\n"
                        "  log4om-migrate --user log4om --password secret \\\n"
                        "                 --station NF0T --out ~/nf0t.db"));
    parser.addHelpOption();
    parser.addVersionOption();

    // Source options
    parser.addOption({{QStringLiteral("host")},
                      QStringLiteral("Log4OM database host (default: localhost)"),
                      QStringLiteral("host"),
                      QStringLiteral("localhost")});

    parser.addOption({{QStringLiteral("port")},
                      QStringLiteral("Log4OM database port (default: 3306)"),
                      QStringLiteral("port"),
                      QStringLiteral("3306")});

    parser.addOption({{QStringLiteral("database"), QStringLiteral("db")},
                      QStringLiteral("Log4OM database name (default: log4om2)"),
                      QStringLiteral("name"),
                      QStringLiteral("log4om2")});

    parser.addOption({{QStringLiteral("user"), QStringLiteral("u")},
                      QStringLiteral("Database username"),
                      QStringLiteral("user")});

    parser.addOption({{QStringLiteral("password"), QStringLiteral("p")},
                      QStringLiteral("Database password"),
                      QStringLiteral("password")});

    // Filter options
    parser.addOption({{QStringLiteral("station"), QStringLiteral("s")},
                      QStringLiteral("Migrate only QSOs where STATION_CALLSIGN matches (case-insensitive)"),
                      QStringLiteral("callsign")});

    parser.addOption({{QStringLiteral("limit")},
                      QStringLiteral("Maximum number of QSOs to migrate (default: all)"),
                      QStringLiteral("n")});

    parser.addOption({{QStringLiteral("offset")},
                      QStringLiteral("Skip first N QSOs before migrating (useful with --limit)"),
                      QStringLiteral("n")});

    // Destination
    parser.addOption({{QStringLiteral("out"), QStringLiteral("o")},
                      QStringLiteral("Destination SQLite path (default: migrated_log.db in current dir)"),
                      QStringLiteral("path")});

    // Behaviour
    parser.addOption({{QStringLiteral("dry-run"), QStringLiteral("n")},
                      QStringLiteral("Read and map QSOs but do not write to destination")});

    parser.process(app);

    MigrateOptions opts;
    opts.host             = parser.value(QStringLiteral("host"));
    opts.port             = parser.value(QStringLiteral("port")).toInt();
    opts.database         = parser.value(QStringLiteral("database"));
    opts.username         = parser.value(QStringLiteral("user"));
    opts.password         = parser.value(QStringLiteral("password"));
    opts.stationCallsign  = parser.value(QStringLiteral("station"));
    opts.destSqlitePath   = parser.value(QStringLiteral("out"));
    opts.dryRun           = parser.isSet(QStringLiteral("dry-run"));

    if (parser.isSet(QStringLiteral("limit")))
        opts.limit  = parser.value(QStringLiteral("limit")).toInt();
    if (parser.isSet(QStringLiteral("offset")))
        opts.offset = parser.value(QStringLiteral("offset")).toInt();

    if (opts.username.isEmpty()) {
        QTextStream err(stderr);
        err << "Error: --user is required.\n";
        return 1;
    }

    QTextStream out(stdout);

    out << "Source : " << opts.host << ":" << opts.port
        << "/" << opts.database << "\n";
    if (!opts.stationCallsign.isEmpty())
        out << "Filter : STATION_CALLSIGN = " << opts.stationCallsign << "\n";
    if (opts.dryRun)
        out << "Mode   : DRY RUN (no writes)\n";
    else
        out << "Dest   : " << (opts.destSqlitePath.isEmpty()
                                    ? QStringLiteral("migrated_log.db")
                                    : opts.destSqlitePath) << "\n";
    out << "\n";
    out.flush();

    Log4OmMigrator migrator;

    // Print progress dots every 100 rows
    QObject::connect(&migrator, &Log4OmMigrator::progress,
                     [&out](int done, int total) {
        if (total > 0) {
            out << QStringLiteral("Reading Log4OM database (%1 rows)...\n").arg(total);
            out.flush();
        } else if (done > 0) {
            out << QStringLiteral("  %1 rows processed...\n").arg(done);
            out.flush();
        }
    });

    const MigrateResult result = migrator.migrate(opts);

    out << "\n--- Migration complete ---\n";
    out << "Read     : " << result.read     << "\n";
    out << "Inserted : " << result.inserted << "\n";
    out << "Skipped  : " << result.skipped  << " (duplicates)\n";
    out << "Errors   : " << result.errors   << "\n";

    if (!result.errorDetails.isEmpty()) {
        out << "\nError details:\n";
        for (const QString &msg : result.errorDetails)
            out << "  " << msg << "\n";
    }

    return result.errors > 0 ? 1 : 0;
}
