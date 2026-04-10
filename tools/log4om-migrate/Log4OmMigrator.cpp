#include "Log4OmMigrator.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTimeZone>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "database/SqliteBackend.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Log4OmMigrator::Log4OmMigrator(QObject *parent)
    : QObject(parent)
{}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

MigrateResult Log4OmMigrator::migrate(const MigrateOptions &opts)
{
    MigrateResult result;

    // -----------------------------------------------------------------------
    // Open source (Log4OM MariaDB)
    // -----------------------------------------------------------------------
    const QString srcConn = QStringLiteral("log4om_src");
    {
        QSqlDatabase src = QSqlDatabase::addDatabase(QStringLiteral("QMYSQL"), srcConn);
        src.setHostName(opts.host);
        src.setPort(opts.port);
        src.setDatabaseName(opts.database);
        src.setUserName(opts.username);
        src.setPassword(opts.password);
        src.setConnectOptions(QStringLiteral(
            "MYSQL_OPT_RECONNECT=1;MYSQL_SET_CHARSET_NAME=utf8mb4"));

        if (!src.open()) {
            result.errorDetails << QStringLiteral("Cannot connect to Log4OM database: %1")
                                       .arg(src.lastError().text());
            ++result.errors;
            src = QSqlDatabase(); // release handle before removeDatabase
            QSqlDatabase::removeDatabase(srcConn);
            return result;
        }
    }

    // -----------------------------------------------------------------------
    // Open destination (NF0T Logger SQLite)
    // -----------------------------------------------------------------------
    SqliteBackend dest;
    if (!opts.dryRun) {
        const QString destPath = opts.destSqlitePath.isEmpty()
                                     ? QStringLiteral("migrated_log.db")
                                     : opts.destSqlitePath;
        if (!dest.open({{QStringLiteral("path"), destPath}})) {
            result.errorDetails << QStringLiteral("Cannot open destination database: %1")
                                       .arg(dest.lastError());
            ++result.errors;
            QSqlDatabase::removeDatabase(srcConn);
            return result;
        }
        if (!dest.initSchema()) {
            result.errorDetails << QStringLiteral("Schema init failed: %1").arg(dest.lastError());
            ++result.errors;
            QSqlDatabase::removeDatabase(srcConn);
            return result;
        }
    }

    // -----------------------------------------------------------------------
    // Count rows for progress reporting
    // -----------------------------------------------------------------------
    {
        QSqlDatabase src = QSqlDatabase::database(srcConn);
        QString countSql = QStringLiteral("SELECT COUNT(*) FROM log");
        if (!opts.stationCallsign.isEmpty())
            countSql += QStringLiteral(" WHERE stationcallsign = ?");

        QSqlQuery countQ(src);
        countQ.prepare(countSql);
        if (!opts.stationCallsign.isEmpty())
            countQ.addBindValue(opts.stationCallsign);
        countQ.exec();
        if (countQ.next())
            emit progress(0, countQ.value(0).toInt());
    }

    // -----------------------------------------------------------------------
    // Fetch and migrate rows
    // -----------------------------------------------------------------------
    {
        QSqlDatabase src = QSqlDatabase::database(srcConn);

        QString sql = QStringLiteral(
            "SELECT qsoid, callsign, band, mode, qsodate, qsoenddate, "
            "address, age, aindex, antaz, antel, antpath, antenna, arrlcheck, arrlsect, "
            "bandrx, callsignurl, class, cnty, comment, cont, "
            "contactedop, contestid, country, cqzone, distance, dxcc, eqcall, "
            "email, freq, freqrx, gridsquare, ituzone, kindex, lat, lon, "
            "maxbursts, msshower, mycity, mycnty, mycountry, mycqzone, mydxcc, "
            "mygridsquare, myituZone, mylat, mylon, myname, mypostalcode, "
            "myrig, mysig, mysiginfo, mystate, mystreet, "
            "name, notes, nrbursts, nrpings, operator, ownercallsign, pfx, "
            "precedence, propmode, qslmsg, qslvia, qsoconfirmations, qsocomplete, "
            "qth, rstrcvd, rstsent, rxpwr, satmode, satname, sfi, sig, siginfo, "
            "srx, srxstring, state, stationcallsign, stx, stxstring, "
            "sunspots, txpwr, swl "
            "FROM log");

        if (!opts.stationCallsign.isEmpty())
            sql += QStringLiteral(" WHERE stationcallsign = ?");

        sql += QStringLiteral(" ORDER BY qsodate ASC");

        if (opts.limit > 0)
            sql += QStringLiteral(" LIMIT %1").arg(opts.limit);
        if (opts.offset > 0)
            sql += QStringLiteral(" OFFSET %1").arg(opts.offset);

        QSqlQuery q(src);
        q.setForwardOnly(true);
        q.prepare(sql);
        if (!opts.stationCallsign.isEmpty())
            q.addBindValue(opts.stationCallsign);

        if (!q.exec()) {
            result.errorDetails << QStringLiteral("Query failed: %1").arg(q.lastError().text());
            ++result.errors;
            QSqlDatabase::removeDatabase(srcConn);
            return result;
        }

        int done = 0;
        while (q.next()) {
            ++result.read;
            Qso qso = rowToQso(q);

            if (!opts.dryRun) {
                if (dest.insertQso(qso)) {
                    ++result.inserted;
                } else {
                    const QString err = dest.lastError();
                    if (err.contains(QLatin1String("UNIQUE"), Qt::CaseInsensitive) ||
                        err.contains(QLatin1String("Duplicate"), Qt::CaseInsensitive)) {
                        ++result.skipped;
                    } else {
                        ++result.errors;
                        result.errorDetails << QStringLiteral("Insert failed for %1 @ %2: %3")
                                                   .arg(qso.callsign,
                                                        qso.datetimeOn.toString(Qt::ISODate),
                                                        err);
                    }
                }
            }

            ++done;
            if (done % 100 == 0)
                emit progress(done, 0);
        }
    }

    QSqlDatabase::removeDatabase(srcConn);
    return result;
}

// ---------------------------------------------------------------------------
// Row → Qso mapping
// ---------------------------------------------------------------------------

Qso Log4OmMigrator::rowToQso(const QSqlQuery &q)
{
    Qso qso;

    auto str  = [&](const char *col) { return q.value(QLatin1String(col)).toString(); };
    auto dbl  = [&](const char *col) -> std::optional<double> {
        const QVariant v = q.value(QLatin1String(col));
        return v.isNull() ? std::optional<double>{} : std::optional<double>{v.toDouble()};
    };
    auto intO = [&](const char *col) -> std::optional<int> {
        const QVariant v = q.value(QLatin1String(col));
        return v.isNull() ? std::optional<int>{} : std::optional<int>{v.toInt()};
    };

    // Core contact fields
    qso.callsign    = str("callsign").toUpper();
    qso.band        = str("band").toLower();
    qso.mode        = str("mode").toUpper();
    qso.datetimeOn  = q.value(QLatin1String("qsodate")).toDateTime().toTimeZone(QTimeZone::utc());
    qso.datetimeOff = q.value(QLatin1String("qsoenddate")).toDateTime();
    if (!qso.datetimeOff.isNull())
        qso.datetimeOff = qso.datetimeOff.toTimeZone(QTimeZone::utc());

    // Frequency
    qso.freq = q.value(QLatin1String("freq")).toDouble();
    if (const auto v = dbl("freqrx"); v && *v > 0.0) qso.freqRx = v;

    // Exchange
    qso.rstSent = str("rstsent");
    qso.rstRcvd = str("rstrcvd");

    // Operator / station info
    qso.name            = str("name");
    qso.qth             = str("qth");
    qso.country         = str("country");
    qso.cont            = str("cont");
    qso.gridsquare      = str("gridsquare").toUpper();
    qso.dxcc            = q.value(QLatin1String("dxcc")).toInt();
    qso.cqZone          = q.value(QLatin1String("cqzone")).toInt();
    qso.ituZone         = q.value(QLatin1String("ituzone")).toInt();
    qso.pfx             = str("pfx");
    qso.state           = str("state");
    qso.county          = str("cnty");
    qso.arrlSect        = str("arrlsect");
    qso.age             = intO("age");
    qso.lat             = dbl("lat");
    qso.lon             = dbl("lon");
    qso.distance        = dbl("distance");
    qso.eqCall          = str("eqcall");
    qso.contactedOp     = str("contactedop");

    // My station
    qso.stationCallsign = str("stationcallsign");
    qso.operatorCall    = str("operator");
    qso.ownerCallsign   = str("ownercallsign");
    qso.myName          = str("myname");
    qso.myCity          = str("mycity");
    qso.myState         = str("mystate");
    qso.myCounty        = str("mycnty");
    qso.myCountry       = str("mycountry");
    qso.myGridsquare    = str("mygridsquare").toUpper();
    qso.myCqZone        = q.value(QLatin1String("mycqzone")).toInt();
    qso.myItuZone       = q.value(QLatin1String("myituZone")).toInt();
    qso.myDxcc          = q.value(QLatin1String("mydxcc")).toInt();
    qso.myLat           = dbl("mylat");
    qso.myLon           = dbl("mylon");
    qso.rig             = str("myrig");
    qso.mySig           = str("mysig");
    qso.mySigInfo       = str("mysiginfo");

    // Equipment / propagation
    qso.antenna     = str("antenna");
    qso.antPath     = str("antpath");
    qso.antAz       = dbl("antaz");
    qso.antEl       = dbl("antel");
    qso.txPwr       = dbl("txpwr");
    qso.rxPwr       = dbl("rxpwr");
    qso.propMode    = str("propmode");
    qso.satMode     = str("satmode");
    qso.satName     = str("satname");

    // Contest / activity
    qso.contestId   = str("contestid");
    qso.arrlCheck   = str("arrlcheck");
    qso.precedence  = str("precedence");
    qso.srx         = intO("srx");
    qso.srxString   = str("srxstring");
    qso.stx         = intO("stx");
    qso.stxString   = str("stxstring");
    qso.sig         = str("sig");
    qso.sigInfo     = str("siginfo");
    qso.msShower    = str("msshower");
    qso.nrBursts    = intO("nrbursts");
    qso.nrPings     = intO("nrpings");
    qso.maxBursts   = dbl("maxbursts");

    // Space weather
    qso.sfi         = dbl("sfi");
    qso.aIndex      = dbl("aindex");
    qso.kIndex      = dbl("kindex");
    qso.sunspots    = intO("sunspots");

    // QSL admin
    qso.qslMsg      = str("qslmsg");
    qso.qslVia      = str("qslvia");
    qso.qsoComplete = str("qsocomplete");

    // Notes / comment
    qso.comment     = str("comment");
    qso.notes       = str("notes");
    // swl is in Log4OM but not in our Qso struct — skip

    // QSL confirmations (Log4OM stores these as JSON)
    parseQsoConfirmations(str("qsoconfirmations"), qso);

    return qso;
}

// ---------------------------------------------------------------------------
// JSON QSL parser
//
// Log4OM v2 stores qsoconfirmations as a JSON array of objects:
//   [
//     { "CT": "LOTW",    "S": "Yes", "R": "No", "SD": "2023-06-17T00:00:00Z" },
//     { "CT": "EQSL",    "S": "Yes", "R": "No", "SD": "2023-06-17T00:00:00Z" },
//     { "CT": "QRZCOM",  "S": "Yes", "R": "No", "SD": "2023-10-21T20:53:40Z" },
//     { "CT": "CLUBLOG", "S": "Yes", "R": "No", "SD": "2023-12-22T00:00:00Z" },
//     { "CT": "QSL",     "S": "Yes", "R": "No" },
//     { "CT": "HAMQTH",  "S": "No",  "R": "No" },
//     { "CT": "HRDLOG",  "S": "No",  "R": "No" }
//   ]
//
// "S"/"R" are "Yes"/"No". "SD"/"RD" are ISO 8601 datetime strings (date part used).
// QSL (paper card), HAMQTH, and HRDLOG have no corresponding Qso fields — skipped.
// ---------------------------------------------------------------------------

static QDate parseIsoDatetime(const QString &s)
{
    // Take the first 10 characters (YYYY-MM-DD) from an ISO 8601 datetime string
    if (s.length() >= 10)
        return QDate::fromString(s.left(10), Qt::ISODate);
    return {};
}

void Log4OmMigrator::parseQsoConfirmations(const QString &json, Qso &qso)
{
    if (json.isEmpty()) return;

    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) return;

    for (const QJsonValue &val : doc.array()) {
        if (!val.isObject()) continue;
        const QJsonObject o = val.toObject();

        const QString ct   = o.value(QLatin1String("CT")).toString();
        const QChar   sent = o.value(QLatin1String("S")).toString() == QLatin1String("Yes") ? 'Y' : 'N';
        const QChar   rcvd = o.value(QLatin1String("R")).toString() == QLatin1String("Yes") ? 'Y' : 'N';
        const QDate   sd   = parseIsoDatetime(o.value(QLatin1String("SD")).toString());
        const QDate   rd   = parseIsoDatetime(o.value(QLatin1String("RD")).toString());

        if (ct == QLatin1String("LOTW")) {
            qso.lotwQslSent = sent;
            qso.lotwQslRcvd = rcvd;
            if (sd.isValid()) qso.lotwSentDate = sd;
            if (rd.isValid()) qso.lotwRcvdDate = rd;
        } else if (ct == QLatin1String("EQSL")) {
            qso.eqslQslSent = sent;
            qso.eqslQslRcvd = rcvd;
            if (sd.isValid()) qso.eqslSentDate = sd;
            if (rd.isValid()) qso.eqslRcvdDate = rd;
        } else if (ct == QLatin1String("QRZCOM")) {
            qso.qrzQslSent = sent;
            qso.qrzQslRcvd = rcvd;
            if (sd.isValid()) qso.qrzSentDate = sd;
            if (rd.isValid()) qso.qrzRcvdDate = rd;
        } else if (ct == QLatin1String("CLUBLOG")) {
            qso.clublogQslSent = sent;
            if (sd.isValid()) qso.clublogSentDate = sd;
        } else if (ct == QLatin1String("QSL")) {
            qso.qslSent = sent;
            qso.qslRcvd = rcvd;
            if (sd.isValid()) qso.qslSentDate = sd;
            if (rd.isValid()) qso.qslRcvdDate = rd;
        }
        // HAMQTH, HRDLOG — no corresponding Qso fields, skip
    }
}
