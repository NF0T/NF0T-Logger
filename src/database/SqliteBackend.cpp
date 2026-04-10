#include "SqliteBackend.h"

#include <expected>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

std::expected<void, QString> SqliteBackend::open(const QVariantMap &config)
{
    const QString path = config.value("path").toString();
    if (path.isEmpty())
        return std::unexpected(QStringLiteral("SqliteBackend: 'path' config key is required"));

    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(path);

    if (!m_db.open())
        return std::unexpected(m_db.lastError().text());

    // Enable WAL mode and foreign keys for better performance/correctness
    execQuery("PRAGMA journal_mode=WAL");
    execQuery("PRAGMA foreign_keys=ON");
    execQuery("PRAGMA synchronous=NORMAL");

    return {};
}

std::expected<void, QString> SqliteBackend::initSchema()
{
    if (auto r = execQuery(R"(
        CREATE TABLE IF NOT EXISTS schema_version (
            version     INTEGER NOT NULL,
            applied_at  TEXT    NOT NULL DEFAULT (datetime('now'))
        )
    )"); !r) return r;

    // Determine current version
    QSqlQuery vq(m_db);
    vq.exec("SELECT MAX(version) FROM schema_version");
    const int currentVersion = (vq.next() && !vq.value(0).isNull()) ? vq.value(0).toInt() : 0;

    if (currentVersion >= 1)
        return {};   // Up to date — add future migrations here

    // --- v1: main schema ---
    if (auto r = execQuery(R"(
        CREATE TABLE IF NOT EXISTS qsos (
            id                  INTEGER PRIMARY KEY AUTOINCREMENT,

            callsign            TEXT    NOT NULL,
            datetime_on         TEXT    NOT NULL,
            band                TEXT    NOT NULL,
            freq                REAL    NOT NULL DEFAULT 0,
            mode                TEXT    NOT NULL,

            datetime_off        TEXT,
            submode             TEXT,
            freq_rx             REAL,
            band_rx             TEXT,

            rst_sent            TEXT,
            rst_rcvd            TEXT,

            name                TEXT,
            contacted_op        TEXT,
            qth                 TEXT,
            gridsquare          TEXT,
            country             TEXT,
            state               TEXT,
            county              TEXT,
            cont                TEXT,
            dxcc                INTEGER,
            cq_zone             INTEGER,
            itu_zone            INTEGER,
            pfx                 TEXT,
            arrl_sect           TEXT,
            lat                 REAL,
            lon                 REAL,
            distance            REAL,
            age                 INTEGER,
            eq_call             TEXT,

            station_callsign    TEXT    NOT NULL DEFAULT '',
            operator_call       TEXT,
            owner_callsign      TEXT,
            my_name             TEXT,
            my_gridsquare       TEXT,
            my_city             TEXT,
            my_state            TEXT,
            my_county           TEXT,
            my_country          TEXT,
            my_dxcc             INTEGER,
            my_cq_zone          INTEGER,
            my_itu_zone         INTEGER,
            my_lat              REAL,
            my_lon              REAL,
            tx_pwr              REAL,
            rx_pwr              REAL,
            antenna             TEXT,
            ant_az              REAL,
            ant_el              REAL,
            ant_path            TEXT,
            rig                 TEXT,

            lotw_qsl_sent       TEXT    DEFAULT 'N',
            lotw_qsl_rcvd       TEXT    DEFAULT 'N',
            lotw_sent_date      TEXT,
            lotw_rcvd_date      TEXT,
            eqsl_qsl_sent       TEXT    DEFAULT 'N',
            eqsl_qsl_rcvd       TEXT    DEFAULT 'N',
            eqsl_sent_date      TEXT,
            eqsl_rcvd_date      TEXT,
            qrz_qsl_sent        TEXT    DEFAULT 'N',
            qrz_qsl_rcvd        TEXT    DEFAULT 'N',
            qrz_sent_date       TEXT,
            qrz_rcvd_date       TEXT,
            clublog_qsl_sent    TEXT    DEFAULT 'N',
            clublog_sent_date   TEXT,
            qsl_sent            TEXT    DEFAULT 'N',
            qsl_rcvd            TEXT    DEFAULT 'N',
            qsl_sent_date       TEXT,
            qsl_rcvd_date       TEXT,
            qsl_via             TEXT,
            qsl_msg             TEXT,

            prop_mode           TEXT,
            sfi                 REAL,
            a_index             REAL,
            k_index             REAL,
            sunspots            INTEGER,

            contest_id          TEXT,
            srx                 INTEGER,
            srx_string          TEXT,
            stx                 INTEGER,
            stx_string          TEXT,
            precedence          TEXT,
            contest_class       TEXT,
            arrl_check          TEXT,

            sig                 TEXT,
            sig_info            TEXT,
            my_sig              TEXT,
            my_sig_info         TEXT,

            sat_name            TEXT,
            sat_mode            TEXT,
            max_bursts          REAL,
            nr_bursts           INTEGER,
            nr_pings            INTEGER,
            ms_shower           TEXT,

            comment             TEXT,
            notes               TEXT,
            qso_complete        TEXT,

            UNIQUE (callsign, datetime_on, band, mode)
        )
    )"); !r) return r;

    // Indexes
    execQuery("CREATE INDEX IF NOT EXISTS idx_qsos_datetime_on    ON qsos(datetime_on)");
    execQuery("CREATE INDEX IF NOT EXISTS idx_qsos_callsign       ON qsos(callsign)");
    execQuery("CREATE INDEX IF NOT EXISTS idx_qsos_band_mode      ON qsos(band, mode)");
    execQuery("CREATE INDEX IF NOT EXISTS idx_qsos_dxcc           ON qsos(dxcc)");
    execQuery("CREATE INDEX IF NOT EXISTS idx_qsos_lotw_sent      ON qsos(lotw_qsl_sent)");
    execQuery("CREATE INDEX IF NOT EXISTS idx_qsos_lotw_rcvd      ON qsos(lotw_qsl_rcvd)");
    execQuery("CREATE INDEX IF NOT EXISTS idx_qsos_eqsl_sent      ON qsos(eqsl_qsl_sent)");
    execQuery("CREATE INDEX IF NOT EXISTS idx_qsos_station_call   ON qsos(station_callsign)");
    execQuery("CREATE INDEX IF NOT EXISTS idx_qsos_sig_info       ON qsos(sig, sig_info)");

    // Record version
    QSqlQuery vins(m_db);
    vins.prepare("INSERT INTO schema_version (version) VALUES (:v)");
    vins.bindValue(":v", 1);
    if (!vins.exec())
        return std::unexpected(vins.lastError().text());

    return {};
}
