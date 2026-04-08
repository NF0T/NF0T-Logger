#include "AdifParser.h"

#include <QFile>
#include <QTextStream>
#include <QTimeZone>

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

AdifParser::Result AdifParser::parseFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Result r;
        r.warnings << QString("Cannot open file: %1").arg(path);
        return r;
    }
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    return parseString(in.readAll());
}

AdifParser::Result AdifParser::parseString(const QString &adif)
{
    Result result;
    const QList<Token> tokens = tokenise(adif);

    bool pastHeader = false;
    FieldMap fields;

    for (const Token &tok : tokens) {
        if (tok.isEoh) {
            pastHeader = true;
            continue;
        }
        if (tok.isEor) {
            // If no <EOH> was found we still process (headerless files)
            if (!fields.isEmpty()) {
                if (fields.contains("CALL") && fields.contains("QSO_DATE") && fields.contains("TIME_ON")) {
                    result.qsos << toQso(fields, result.warnings);
                } else {
                    ++result.skipped;
                    result.warnings << QString("Record skipped — missing CALL, QSO_DATE, or TIME_ON "
                                               "(has: %1)").arg(fields.keys().join(", "));
                }
            }
            fields.clear();
            pastHeader = true;  // treat first EOR as implicit EOH
            continue;
        }
        if (pastHeader)
            fields.insert(tok.name, tok.value);
    }

    return result;
}

// ---------------------------------------------------------------------------
// Tokeniser — spec-correct length-based parsing
// ---------------------------------------------------------------------------

QList<AdifParser::Token> AdifParser::tokenise(const QString &text)
{
    QList<Token> tokens;
    int pos = 0;
    const int len = text.length();

    while (pos < len) {
        const int ltPos = text.indexOf('<', pos);
        if (ltPos == -1) break;

        const int gtPos = text.indexOf('>', ltPos);
        if (gtPos == -1) break;

        // Tag content between < and >
        const QString tag = text.mid(ltPos + 1, gtPos - ltPos - 1);
        pos = gtPos + 1;

        const QStringList parts = tag.split(':');
        const QString name = parts[0].toUpper().trimmed();

        if (name == "EOH") { tokens.append({"EOH", {}, true,  false}); continue; }
        if (name == "EOR") { tokens.append({"EOR", {}, false, true });  continue; }

        if (parts.size() < 2) continue;  // malformed tag with no length — skip

        bool ok = false;
        const int fieldLen = parts[1].trimmed().toInt(&ok);
        if (!ok || fieldLen < 0) continue;

        const QString value = text.mid(pos, fieldLen);
        pos += fieldLen;

        tokens.append({name, value, false, false});
    }

    return tokens;
}

// ---------------------------------------------------------------------------
// Record → Qso
// ---------------------------------------------------------------------------

Qso AdifParser::toQso(const FieldMap &f, QStringList &warn)
{
    auto val = [&](const char *k) -> QString {
        auto it = f.constFind(QString::fromLatin1(k));
        return it != f.constEnd() ? it.value() : QString();
    };
    auto hasKey = [&](const char *k) { return f.contains(QString::fromLatin1(k)); };

    Qso q;

    // --- Core ---
    q.callsign    = val("CALL").toUpper().trimmed();
    q.datetimeOn  = parseDateTime(val("QSO_DATE"), val("TIME_ON"));
    q.datetimeOff = parseDateTime(val("QSO_DATE_OFF"), val("TIME_OFF"));
    q.band        = val("BAND").toLower();
    q.freq        = val("FREQ").toDouble();
    q.mode        = val("MODE").toUpper();
    q.submode     = val("SUBMODE").toUpper();
    q.bandRx      = val("BAND_RX").toLower();
    const double freqRxRaw = val("FREQ_RX").toDouble();
    if (freqRxRaw > 0) q.freqRx = freqRxRaw;

    // --- RST ---
    q.rstSent = val("RST_SENT");
    q.rstRcvd = val("RST_RCVD");

    // --- Contacted station ---
    q.name        = val("NAME");
    q.contactedOp = val("CONTACTED_OP");
    q.qth         = val("QTH");
    q.gridsquare  = val("GRIDSQUARE").toUpper();
    q.country     = val("COUNTRY");
    q.state       = val("STATE").toUpper();
    q.county      = val("CNTY");
    q.cont        = val("CONT").toUpper();
    q.dxcc        = val("DXCC").toInt();
    q.cqZone      = val("CQZ").toInt();
    q.ituZone     = val("ITUZ").toInt();
    q.pfx         = val("PFX").toUpper();
    q.arrlSect    = val("ARRL_SECT").toUpper();
    q.lat         = parseLatLon(val("LAT"));
    q.lon         = parseLatLon(val("LON"));
    const double distRaw = val("DISTANCE").toDouble();
    if (distRaw > 0) q.distance = distRaw;
    const int ageRaw = val("AGE").toInt();
    if (ageRaw > 0) q.age = ageRaw;
    q.eqCall      = val("EQ_CALL").toUpper();

    // --- My station ---
    q.stationCallsign = val("STATION_CALLSIGN").toUpper();
    q.operatorCall    = val("OPERATOR").toUpper();
    q.ownerCallsign   = val("OWNER_CALLSIGN").toUpper();
    q.myName          = val("MY_NAME");
    q.myGridsquare    = val("MY_GRIDSQUARE").toUpper();
    q.myCity          = val("MY_CITY");
    q.myState         = val("MY_STATE").toUpper();
    q.myCounty        = val("MY_CNTY");
    q.myCountry       = val("MY_COUNTRY");
    q.myDxcc          = val("MY_DXCC").toInt();
    q.myCqZone        = val("MY_CQ_ZONE").toInt();
    q.myItuZone       = val("MY_ITU_ZONE").toInt();
    q.myLat           = parseLatLon(val("MY_LAT"));
    q.myLon           = parseLatLon(val("MY_LON"));
    const double txPwrRaw = val("TX_PWR").toDouble();
    if (txPwrRaw >= 0 && hasKey("TX_PWR")) q.txPwr = txPwrRaw;
    const double rxPwrRaw = val("RX_PWR").toDouble();
    if (rxPwrRaw >= 0 && hasKey("RX_PWR")) q.rxPwr = rxPwrRaw;
    q.antenna = val("ANTENNA");
    const double antAzRaw = val("ANT_AZ").toDouble();
    if (hasKey("ANT_AZ")) q.antAz = antAzRaw;
    const double antElRaw = val("ANT_EL").toDouble();
    if (hasKey("ANT_EL")) q.antEl = antElRaw;
    q.antPath = val("ANT_PATH").toUpper();
    q.rig     = val("MY_RIG");

    // --- QSL ---
    q.lotwQslSent  = parseQslStatus(val("LOTW_QSL_SENT"));
    q.lotwQslRcvd  = parseQslStatus(val("LOTW_QSL_RCVD"));
    q.lotwSentDate = parseAdifDate(val("LOTW_QSLSDATE"));
    q.lotwRcvdDate = parseAdifDate(val("LOTW_QSLRDATE"));

    q.eqslQslSent  = parseQslStatus(val("EQSL_QSL_SENT"));
    q.eqslQslRcvd  = parseQslStatus(val("EQSL_QSL_RCVD"));
    q.eqslSentDate = parseAdifDate(val("EQSL_QSLSDATE"));
    q.eqslRcvdDate = parseAdifDate(val("EQSL_QSLRDATE"));

    q.qslSent     = parseQslStatus(val("QSL_SENT"));
    q.qslRcvd     = parseQslStatus(val("QSL_RCVD"));
    q.qslSentDate = parseAdifDate(val("QSL_SDATE"));
    q.qslRcvdDate = parseAdifDate(val("QSL_RDATE"));
    q.qslVia      = val("QSL_VIA");
    q.qslMsg      = val("QSLMSG");

    // NF0T Logger APP_ extensions for QRZ and ClubLog
    q.qrzQslSent    = parseQslStatus(val("APP_NF0T_QRZ_QSL_SENT"));
    q.qrzQslRcvd    = parseQslStatus(val("APP_NF0T_QRZ_QSL_RCVD"));
    q.qrzSentDate   = parseAdifDate(val("APP_NF0T_QRZ_QSLSDATE"));
    q.qrzRcvdDate   = parseAdifDate(val("APP_NF0T_QRZ_QSLRDATE"));
    q.clublogQslSent = parseQslStatus(val("APP_NF0T_CLUBLOG_QSL_SENT"));
    q.clublogSentDate = parseAdifDate(val("APP_NF0T_CLUBLOG_QSLSDATE"));

    // --- Propagation ---
    q.propMode = val("PROP_MODE").toUpper();
    const double sfiRaw = val("SFI").toDouble();
    if (hasKey("SFI") && sfiRaw > 0) q.sfi = sfiRaw;
    const double aIdxRaw = val("A_INDEX").toDouble();
    if (hasKey("A_INDEX")) q.aIndex = aIdxRaw;
    const double kIdxRaw = val("K_INDEX").toDouble();
    if (hasKey("K_INDEX")) q.kIndex = kIdxRaw;
    const int snRaw = val("SUNSPOT_SMOOTHED_COUNT").toInt();
    if (hasKey("SUNSPOT_SMOOTHED_COUNT")) q.sunspots = snRaw;

    // --- Contest ---
    q.contestId    = val("CONTEST_ID").toUpper();
    const int srxRaw = val("SRX").toInt();
    if (hasKey("SRX")) q.srx = srxRaw;
    q.srxString    = val("SRX_STRING");
    const int stxRaw = val("STX").toInt();
    if (hasKey("STX")) q.stx = stxRaw;
    q.stxString    = val("STX_STRING");
    q.precedence   = val("PRECEDENCE").toUpper();
    q.contestClass = val("CLASS").toUpper();
    q.arrlCheck    = val("ARRL_SECT").toUpper();  // ARRL_SECT doubles as section in Sweepstakes

    // --- Special activities ---
    q.sig       = val("SIG").toUpper();
    q.sigInfo   = val("SIG_INFO").toUpper();
    q.mySig     = val("MY_SIG").toUpper();
    q.mySigInfo = val("MY_SIG_INFO").toUpper();

    // --- Satellite / meteor scatter ---
    q.satName  = val("SAT_NAME");
    q.satMode  = val("SAT_MODE").toUpper();
    const double maxBurstRaw = val("MAX_BURSTS").toDouble();
    if (hasKey("MAX_BURSTS")) q.maxBursts = maxBurstRaw;
    const int nrBurstRaw = val("NR_BURSTS").toInt();
    if (hasKey("NR_BURSTS")) q.nrBursts = nrBurstRaw;
    const int nrPingRaw = val("NR_PINGS").toInt();
    if (hasKey("NR_PINGS")) q.nrPings = nrPingRaw;
    q.msShower = val("MS_SHOWER");

    // --- Free text ---
    q.comment     = val("COMMENT");
    q.notes       = val("NOTES");
    q.qsoComplete = val("QSO_COMPLETE");

    if (!q.datetimeOn.isValid())
        warn << QString("Record for %1: datetime_on could not be parsed from '%2'/'%3'")
                .arg(q.callsign, val("QSO_DATE"), val("TIME_ON"));

    return q;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QDateTime AdifParser::parseDateTime(const QString &date, const QString &time)
{
    if (date.isEmpty()) return {};
    // Date: YYYYMMDD, Time: HHMMSS or HHMM
    QString timeStr = time.trimmed();
    if (timeStr.length() == 4) timeStr += "00";
    const QString combined = date.trimmed() + timeStr.leftJustified(6, '0');
    QDateTime dt = QDateTime::fromString(combined, "yyyyMMddHHmmss");
    if (dt.isValid()) dt.setTimeZone(QTimeZone::utc());
    return dt;
}

QDate AdifParser::parseAdifDate(const QString &s)
{
    if (s.isEmpty()) return {};
    return QDate::fromString(s.trimmed(), "yyyyMMdd");
}

QChar AdifParser::parseQslStatus(const QString &s)
{
    if (s.isEmpty()) return QChar('N');
    const QChar c = s.trimmed().at(0).toUpper();
    // Valid ADIF QSL statuses: Y N R Q I
    if (QString("YNRQI").contains(c)) return c;
    return QChar('N');
}

std::optional<double> AdifParser::parseLatLon(const QString &adifCoord)
{
    // ADIF format: XDDD MM.MMM  (X = N/S/E/W, DDD = degrees, MM.MMM = minutes)
    const QString s = adifCoord.trimmed();
    if (s.length() < 6) return std::nullopt;

    const QChar dir = s.at(0).toUpper();
    if (!QString("NSEW").contains(dir)) return std::nullopt;

    const int spacePos = s.indexOf(' ', 1);
    if (spacePos == -1) return std::nullopt;

    bool ok1 = false, ok2 = false;
    const double degrees = s.mid(1, spacePos - 1).toDouble(&ok1);
    const double minutes = s.mid(spacePos + 1).toDouble(&ok2);
    if (!ok1 || !ok2) return std::nullopt;

    double value = degrees + minutes / 60.0;
    if (dir == 'S' || dir == 'W') value = -value;
    return value;
}
