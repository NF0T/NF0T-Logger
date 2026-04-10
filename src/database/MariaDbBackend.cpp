#include "MariaDbBackend.h"

#include <expected>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

std::expected<void, QString> MariaDbBackend::open(const QVariantMap &config)
{
    m_db = QSqlDatabase::addDatabase("QMYSQL", m_connectionName);
    m_db.setHostName    (config.value("host",     "127.0.0.1").toString());
    m_db.setPort        (config.value("port",     3306).toInt());
    m_db.setDatabaseName(config.value("database", "nf0t_logger").toString());
    m_db.setUserName    (config.value("username").toString());
    m_db.setPassword    (config.value("password").toString());

    // Enable UTF-8 and strict mode
    m_db.setConnectOptions("MYSQL_OPT_RECONNECT=1;MYSQL_SET_CHARSET_NAME=utf8mb4");

    if (!m_db.open())
        return std::unexpected(m_db.lastError().text());
    return {};
}

std::expected<void, QString> MariaDbBackend::initSchema()
{
    if (auto r = execQuery(R"(
        CREATE TABLE IF NOT EXISTS schema_version (
            version     INT      NOT NULL,
            applied_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )"); !r) return r;

    QSqlQuery vq(m_db);
    vq.exec("SELECT MAX(version) FROM schema_version");
    const int currentVersion = (vq.next() && !vq.value(0).isNull()) ? vq.value(0).toInt() : 0;

    if (currentVersion >= 1)
        return {};

    // --- v1: main schema ---
    if (auto r = execQuery(R"(
        CREATE TABLE IF NOT EXISTS qsos (
            id                  BIGINT          NOT NULL AUTO_INCREMENT PRIMARY KEY,

            callsign            VARCHAR(20)     NOT NULL,
            datetime_on         DATETIME        NOT NULL,
            band                VARCHAR(10)     NOT NULL,
            freq                DECIMAL(12,6)   NOT NULL DEFAULT 0,
            mode                VARCHAR(30)     NOT NULL,

            datetime_off        DATETIME        DEFAULT NULL,
            submode             VARCHAR(30)     DEFAULT NULL,
            freq_rx             DECIMAL(12,6)   DEFAULT NULL,
            band_rx             VARCHAR(10)     DEFAULT NULL,

            rst_sent            VARCHAR(10)     DEFAULT NULL,
            rst_rcvd            VARCHAR(10)     DEFAULT NULL,

            name                VARCHAR(200)    DEFAULT NULL,
            contacted_op        VARCHAR(50)     DEFAULT NULL,
            qth                 VARCHAR(200)    DEFAULT NULL,
            gridsquare          VARCHAR(10)     DEFAULT NULL,
            country             VARCHAR(100)    DEFAULT NULL,
            state               VARCHAR(100)    DEFAULT NULL,
            county              VARCHAR(100)    DEFAULT NULL,
            cont                CHAR(2)         DEFAULT NULL,
            dxcc                INT             DEFAULT NULL,
            cq_zone             INT             DEFAULT NULL,
            itu_zone            INT             DEFAULT NULL,
            pfx                 VARCHAR(20)     DEFAULT NULL,
            arrl_sect           VARCHAR(20)     DEFAULT NULL,
            lat                 DECIMAL(10,7)   DEFAULT NULL,
            lon                 DECIMAL(10,7)   DEFAULT NULL,
            distance            DECIMAL(10,3)   DEFAULT NULL,
            age                 INT             DEFAULT NULL,
            eq_call             VARCHAR(50)     DEFAULT NULL,

            station_callsign    VARCHAR(20)     NOT NULL DEFAULT '',
            operator_call       VARCHAR(20)     DEFAULT NULL,
            owner_callsign      VARCHAR(20)     DEFAULT NULL,
            my_name             VARCHAR(200)    DEFAULT NULL,
            my_gridsquare       VARCHAR(10)     DEFAULT NULL,
            my_city             VARCHAR(100)    DEFAULT NULL,
            my_state            VARCHAR(100)    DEFAULT NULL,
            my_county           VARCHAR(100)    DEFAULT NULL,
            my_country          VARCHAR(100)    DEFAULT NULL,
            my_dxcc             INT             DEFAULT NULL,
            my_cq_zone          INT             DEFAULT NULL,
            my_itu_zone         INT             DEFAULT NULL,
            my_lat              DECIMAL(10,7)   DEFAULT NULL,
            my_lon              DECIMAL(10,7)   DEFAULT NULL,
            tx_pwr              DECIMAL(10,3)   DEFAULT NULL,
            rx_pwr              DECIMAL(10,3)   DEFAULT NULL,
            antenna             VARCHAR(200)    DEFAULT NULL,
            ant_az              DECIMAL(6,2)    DEFAULT NULL,
            ant_el              DECIMAL(6,2)    DEFAULT NULL,
            ant_path            VARCHAR(10)     DEFAULT NULL,
            rig                 VARCHAR(200)    DEFAULT NULL,

            lotw_qsl_sent       CHAR(1)         DEFAULT 'N',
            lotw_qsl_rcvd       CHAR(1)         DEFAULT 'N',
            lotw_sent_date      DATE            DEFAULT NULL,
            lotw_rcvd_date      DATE            DEFAULT NULL,
            eqsl_qsl_sent       CHAR(1)         DEFAULT 'N',
            eqsl_qsl_rcvd       CHAR(1)         DEFAULT 'N',
            eqsl_sent_date      DATE            DEFAULT NULL,
            eqsl_rcvd_date      DATE            DEFAULT NULL,
            qrz_qsl_sent        CHAR(1)         DEFAULT 'N',
            qrz_qsl_rcvd        CHAR(1)         DEFAULT 'N',
            qrz_sent_date       DATE            DEFAULT NULL,
            qrz_rcvd_date       DATE            DEFAULT NULL,
            clublog_qsl_sent    CHAR(1)         DEFAULT 'N',
            clublog_sent_date   DATE            DEFAULT NULL,
            qsl_sent            CHAR(1)         DEFAULT 'N',
            qsl_rcvd            CHAR(1)         DEFAULT 'N',
            qsl_sent_date       DATE            DEFAULT NULL,
            qsl_rcvd_date       DATE            DEFAULT NULL,
            qsl_via             VARCHAR(200)    DEFAULT NULL,
            qsl_msg             VARCHAR(300)    DEFAULT NULL,

            prop_mode           VARCHAR(50)     DEFAULT NULL,
            sfi                 DECIMAL(6,2)    DEFAULT NULL,
            a_index             DECIMAL(6,2)    DEFAULT NULL,
            k_index             DECIMAL(6,2)    DEFAULT NULL,
            sunspots            INT             DEFAULT NULL,

            contest_id          VARCHAR(50)     DEFAULT NULL,
            srx                 INT             DEFAULT NULL,
            srx_string          VARCHAR(50)     DEFAULT NULL,
            stx                 INT             DEFAULT NULL,
            stx_string          VARCHAR(50)     DEFAULT NULL,
            precedence          VARCHAR(20)     DEFAULT NULL,
            contest_class       VARCHAR(20)     DEFAULT NULL,
            arrl_check          VARCHAR(20)     DEFAULT NULL,

            sig                 VARCHAR(50)     DEFAULT NULL,
            sig_info            VARCHAR(200)    DEFAULT NULL,
            my_sig              VARCHAR(50)     DEFAULT NULL,
            my_sig_info         VARCHAR(200)    DEFAULT NULL,

            sat_name            VARCHAR(100)    DEFAULT NULL,
            sat_mode            VARCHAR(50)     DEFAULT NULL,
            max_bursts          DECIMAL(8,3)    DEFAULT NULL,
            nr_bursts           INT             DEFAULT NULL,
            nr_pings            INT             DEFAULT NULL,
            ms_shower           VARCHAR(50)     DEFAULT NULL,

            comment             VARCHAR(500)    DEFAULT NULL,
            notes               TEXT            DEFAULT NULL,
            qso_complete        VARCHAR(20)     DEFAULT NULL,

            UNIQUE KEY uq_qso (callsign, datetime_on, band, mode),
            INDEX idx_datetime_on   (datetime_on),
            INDEX idx_callsign      (callsign),
            INDEX idx_band_mode     (band, mode),
            INDEX idx_dxcc          (dxcc),
            INDEX idx_lotw_sent     (lotw_qsl_sent),
            INDEX idx_lotw_rcvd     (lotw_qsl_rcvd),
            INDEX idx_eqsl_sent     (eqsl_qsl_sent),
            INDEX idx_station_call  (station_callsign),
            INDEX idx_sig_info      (sig, sig_info)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
    )"); !r) return r;

    QSqlQuery vins(m_db);
    vins.prepare("INSERT INTO schema_version (version) VALUES (:v)");
    vins.bindValue(":v", 1);
    if (!vins.exec())
        return std::unexpected(vins.lastError().text());

    return {};
}
