// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "AdifWriter.h"

#include <cmath>
#include <QDateTime>
#include <QFile>
#include <QTextStream>

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool AdifWriter::writeFile(const QString &path, const QList<Qso> &qsos, const Options &opts)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << writeString(qsos, opts);
    return true;
}

QString AdifWriter::writeString(const QList<Qso> &qsos, const Options &opts)
{
    QString out;
    if (opts.includeHeader)
        out += header(opts);
    for (const Qso &q : qsos)
        out += record(q, opts);
    return out;
}

// ---------------------------------------------------------------------------
// Header
// ---------------------------------------------------------------------------

QString AdifWriter::header(const Options &opts)
{
    QString h;
    h += QString("NF0T Logger ADIF Export\n");
    h += f("ADIF_VER",          "3.1.4");
    h += f("CREATED_TIMESTAMP", QDateTime::currentDateTimeUtc().toString("yyyyMMdd HHmmss"));
    h += f("PROGRAMID",         opts.programId);
    h += f("PROGRAMVERSION",    opts.programVersion);
    h += "<EOH>\n\n";
    return h;
}

// ---------------------------------------------------------------------------
// Record
// ---------------------------------------------------------------------------

QString AdifWriter::record(const Qso &qso, const Options &opts)
{
    QString r;

    // Core
    r += f("CALL",             qso.callsign);
    r += f("QSO_DATE",         qso.datetimeOn.toUTC().toString("yyyyMMdd"));
    r += f("TIME_ON",          qso.datetimeOn.toUTC().toString("HHmmss"));
    if (qso.datetimeOff.isValid()) {
        r += f("QSO_DATE_OFF", qso.datetimeOff.toUTC().toString("yyyyMMdd"));
        r += f("TIME_OFF",     qso.datetimeOff.toUTC().toString("HHmmss"));
    }
    r += f("BAND",             qso.band);
    r += f("FREQ",             qso.freq, 6, true);
    r += f("MODE",             qso.mode);
    r += f("SUBMODE",          qso.submode);
    if (qso.freqRx.has_value()) r += f("FREQ_RX", *qso.freqRx, 6);
    r += f("BAND_RX",          qso.bandRx);

    // RST
    r += f("RST_SENT",         qso.rstSent);
    r += f("RST_RCVD",         qso.rstRcvd);

    // Contacted station
    r += f("NAME",             qso.name);
    r += f("CONTACTED_OP",     qso.contactedOp);
    r += f("QTH",              qso.qth);
    r += f("GRIDSQUARE",       qso.gridsquare);
    r += f("COUNTRY",          qso.country);
    r += f("STATE",            qso.state);
    r += f("CNTY",             qso.county);
    r += f("CONT",             qso.cont);
    r += f("DXCC",             qso.dxcc);
    r += f("CQZ",              qso.cqZone);
    r += f("ITUZ",             qso.ituZone);
    r += f("PFX",              qso.pfx);
    r += f("ARRL_SECT",        qso.arrlSect);
    if (qso.lat.has_value())      r += fLatLon("LAT",      *qso.lat,  true);
    if (qso.lon.has_value())      r += fLatLon("LON",      *qso.lon,  false);
    if (qso.distance.has_value()) r += f("DISTANCE",       *qso.distance, 1);
    if (qso.age.has_value())      r += f("AGE",            *qso.age, false);
    r += f("EQ_CALL",          qso.eqCall);

    // My station
    r += f("STATION_CALLSIGN", qso.stationCallsign);
    r += f("OPERATOR",         qso.operatorCall);
    r += f("OWNER_CALLSIGN",   qso.ownerCallsign);
    r += f("MY_NAME",          qso.myName);
    r += f("MY_GRIDSQUARE",    qso.myGridsquare);
    r += f("MY_CITY",          qso.myCity);
    r += f("MY_STATE",         qso.myState);
    r += f("MY_CNTY",          qso.myCounty);
    r += f("MY_COUNTRY",       qso.myCountry);
    r += f("MY_DXCC",          qso.myDxcc);
    r += f("MY_CQ_ZONE",       qso.myCqZone);
    r += f("MY_ITU_ZONE",      qso.myItuZone);
    if (qso.myLat.has_value()) r += fLatLon("MY_LAT", *qso.myLat, true);
    if (qso.myLon.has_value()) r += fLatLon("MY_LON", *qso.myLon, false);
    if (qso.txPwr.has_value()) r += f("TX_PWR",  *qso.txPwr, 1);
    if (qso.rxPwr.has_value()) r += f("RX_PWR",  *qso.rxPwr, 1);
    r += f("ANTENNA",          qso.antenna);
    if (qso.antAz.has_value()) r += f("ANT_AZ",  *qso.antAz, 1);
    if (qso.antEl.has_value()) r += f("ANT_EL",  *qso.antEl, 1);
    r += f("ANT_PATH",         qso.antPath);
    r += f("MY_RIG",           qso.rig);

    // QSL — standard ADIF fields
    r += f("LOTW_QSL_SENT",   qso.lotwQslSent);
    r += f("LOTW_QSL_RCVD",   qso.lotwQslRcvd);
    r += f("LOTW_QSLSDATE",   qso.lotwSentDate);
    r += f("LOTW_QSLRDATE",   qso.lotwRcvdDate);
    r += f("EQSL_QSL_SENT",   qso.eqslQslSent);
    r += f("EQSL_QSL_RCVD",   qso.eqslQslRcvd);
    r += f("EQSL_QSLSDATE",   qso.eqslSentDate);
    r += f("EQSL_QSLRDATE",   qso.eqslRcvdDate);
    r += f("QSL_SENT",        qso.qslSent);
    r += f("QSL_RCVD",        qso.qslRcvd);
    r += f("QSL_SDATE",       qso.qslSentDate);
    r += f("QSL_RDATE",       qso.qslRcvdDate);
    r += f("QSL_VIA",         qso.qslVia);
    r += f("QSLMSG",          qso.qslMsg);

    // QSL — NF0T Logger APP_ extensions for QRZ and ClubLog
    if (opts.includeAppFields) {
        r += f("APP_NF0T_QRZ_QSL_SENT",      qso.qrzQslSent);
        r += f("APP_NF0T_QRZ_QSL_RCVD",      qso.qrzQslRcvd);
        r += f("APP_NF0T_QRZ_QSLSDATE",       qso.qrzSentDate);
        r += f("APP_NF0T_QRZ_QSLRDATE",       qso.qrzRcvdDate);
        r += f("APP_NF0T_CLUBLOG_QSL_SENT",   qso.clublogQslSent);
        r += f("APP_NF0T_CLUBLOG_QSLSDATE",   qso.clublogSentDate);
    }

    // Propagation
    r += f("PROP_MODE",        qso.propMode);
    if (qso.sfi.has_value())    r += f("SFI",     *qso.sfi,    1);
    if (qso.aIndex.has_value()) r += f("A_INDEX", *qso.aIndex, 1);
    if (qso.kIndex.has_value()) r += f("K_INDEX", *qso.kIndex, 1);
    if (qso.sunspots.has_value()) r += f("SUNSPOT_SMOOTHED_COUNT", *qso.sunspots, false);

    // Contest
    r += f("CONTEST_ID",       qso.contestId);
    if (qso.srx.has_value())   r += f("SRX", *qso.srx, false);
    r += f("SRX_STRING",       qso.srxString);
    if (qso.stx.has_value())   r += f("STX", *qso.stx, false);
    r += f("STX_STRING",       qso.stxString);
    r += f("PRECEDENCE",       qso.precedence);
    r += f("CLASS",            qso.contestClass);
    r += f("ARRL_SECT",        qso.arrlCheck);

    // Special activities
    r += f("SIG",              qso.sig);
    r += f("SIG_INFO",         qso.sigInfo);
    r += f("MY_SIG",           qso.mySig);
    r += f("MY_SIG_INFO",      qso.mySigInfo);

    // Satellite / meteor scatter
    r += f("SAT_NAME",         qso.satName);
    r += f("SAT_MODE",         qso.satMode);
    if (qso.maxBursts.has_value()) r += f("MAX_BURSTS", *qso.maxBursts, 1);
    if (qso.nrBursts.has_value())  r += f("NR_BURSTS",  *qso.nrBursts, false);
    if (qso.nrPings.has_value())   r += f("NR_PINGS",   *qso.nrPings,  false);
    r += f("MS_SHOWER",        qso.msShower);

    // Free text
    r += f("COMMENT",          qso.comment);
    r += f("NOTES",            qso.notes);
    r += f("QSO_COMPLETE",     qso.qsoComplete);

    r += "<EOR>\n\n";
    return r;
}

// ---------------------------------------------------------------------------
// Field helpers
// ---------------------------------------------------------------------------

QString AdifWriter::f(const QString &name, const QString &value)
{
    if (value.isEmpty()) return {};
    return QString("<%1:%2>%3\n").arg(name).arg(value.length()).arg(value);
}

QString AdifWriter::f(const QString &name, double value, int decimals, bool skipIfZero)
{
    if (skipIfZero && value == 0.0) return {};
    const QString s = QString::number(value, 'f', decimals);
    return QString("<%1:%2>%3\n").arg(name).arg(s.length()).arg(s);
}

QString AdifWriter::f(const QString &name, int value, bool skipIfZero)
{
    if (skipIfZero && value == 0) return {};
    const QString s = QString::number(value);
    return QString("<%1:%2>%3\n").arg(name).arg(s.length()).arg(s);
}

QString AdifWriter::f(const QString &name, const QDate &date)
{
    if (!date.isValid()) return {};
    const QString s = date.toString("yyyyMMdd");
    return QString("<%1:%2>%3\n").arg(name).arg(s.length()).arg(s);
}

QString AdifWriter::f(const QString &name, QChar status)
{
    // Only write if not the default 'N'
    if (status == QChar('N')) return {};
    const QString s(status);
    return QString("<%1:1>%2\n").arg(name).arg(s);
}

QString AdifWriter::fLatLon(const QString &name, double degrees, bool isLat)
{
    const char dir = isLat ? (degrees >= 0 ? 'N' : 'S') : (degrees >= 0 ? 'E' : 'W');
    const double absDeg = std::abs(degrees);
    const int    intDeg = static_cast<int>(absDeg);
    const double mins   = (absDeg - intDeg) * 60.0;

    // Format: XDDD MM.MMM (11 chars)
    const QString s = QString("%1%2 %3")
                          .arg(dir)
                          .arg(intDeg, 3, 10, QChar('0'))
                          .arg(mins, 6, 'f', 3, QChar('0'));
    return QString("<%1:%2>%3\n").arg(name).arg(s.length()).arg(s);
}
