#include "SqlBackendBase.h"

#include <expected>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QTimeZone>

std::atomic<int> SqlBackendBase::s_connectionCounter{0};

// ---------------------------------------------------------------------------
// SQL templates
// ---------------------------------------------------------------------------

const char *SqlBackendBase::INSERT_SQL = R"(
INSERT INTO qsos (
    callsign, datetime_on, band, freq, mode,
    datetime_off, submode, freq_rx, band_rx,
    rst_sent, rst_rcvd,
    name, contacted_op, qth, gridsquare, country, state, county, cont,
    dxcc, cq_zone, itu_zone, pfx, arrl_sect,
    lat, lon, distance, age, eq_call,
    station_callsign, operator_call, owner_callsign,
    my_name, my_gridsquare, my_city, my_state, my_county, my_country,
    my_dxcc, my_cq_zone, my_itu_zone, my_lat, my_lon,
    tx_pwr, rx_pwr, antenna, ant_az, ant_el, ant_path, rig,
    lotw_qsl_sent, lotw_qsl_rcvd, lotw_sent_date, lotw_rcvd_date,
    eqsl_qsl_sent, eqsl_qsl_rcvd, eqsl_sent_date, eqsl_rcvd_date,
    qrz_qsl_sent, qrz_qsl_rcvd, qrz_sent_date, qrz_rcvd_date,
    clublog_qsl_sent, clublog_sent_date,
    qsl_sent, qsl_rcvd, qsl_sent_date, qsl_rcvd_date, qsl_via, qsl_msg,
    prop_mode, sfi, a_index, k_index, sunspots,
    contest_id, srx, srx_string, stx, stx_string, precedence, contest_class, arrl_check,
    sig, sig_info, my_sig, my_sig_info,
    sat_name, sat_mode, max_bursts, nr_bursts, nr_pings, ms_shower,
    comment, notes, qso_complete
) VALUES (
    :callsign, :datetime_on, :band, :freq, :mode,
    :datetime_off, :submode, :freq_rx, :band_rx,
    :rst_sent, :rst_rcvd,
    :name, :contacted_op, :qth, :gridsquare, :country, :state, :county, :cont,
    :dxcc, :cq_zone, :itu_zone, :pfx, :arrl_sect,
    :lat, :lon, :distance, :age, :eq_call,
    :station_callsign, :operator_call, :owner_callsign,
    :my_name, :my_gridsquare, :my_city, :my_state, :my_county, :my_country,
    :my_dxcc, :my_cq_zone, :my_itu_zone, :my_lat, :my_lon,
    :tx_pwr, :rx_pwr, :antenna, :ant_az, :ant_el, :ant_path, :rig,
    :lotw_qsl_sent, :lotw_qsl_rcvd, :lotw_sent_date, :lotw_rcvd_date,
    :eqsl_qsl_sent, :eqsl_qsl_rcvd, :eqsl_sent_date, :eqsl_rcvd_date,
    :qrz_qsl_sent, :qrz_qsl_rcvd, :qrz_sent_date, :qrz_rcvd_date,
    :clublog_qsl_sent, :clublog_sent_date,
    :qsl_sent, :qsl_rcvd, :qsl_sent_date, :qsl_rcvd_date, :qsl_via, :qsl_msg,
    :prop_mode, :sfi, :a_index, :k_index, :sunspots,
    :contest_id, :srx, :srx_string, :stx, :stx_string, :precedence, :contest_class, :arrl_check,
    :sig, :sig_info, :my_sig, :my_sig_info,
    :sat_name, :sat_mode, :max_bursts, :nr_bursts, :nr_pings, :ms_shower,
    :comment, :notes, :qso_complete
)
)";

const char *SqlBackendBase::UPDATE_SQL = R"(
UPDATE qsos SET
    callsign        = :callsign,
    datetime_on     = :datetime_on,
    band            = :band,
    freq            = :freq,
    mode            = :mode,
    datetime_off    = :datetime_off,
    submode         = :submode,
    freq_rx         = :freq_rx,
    band_rx         = :band_rx,
    rst_sent        = :rst_sent,
    rst_rcvd        = :rst_rcvd,
    name            = :name,
    contacted_op    = :contacted_op,
    qth             = :qth,
    gridsquare      = :gridsquare,
    country         = :country,
    state           = :state,
    county          = :county,
    cont            = :cont,
    dxcc            = :dxcc,
    cq_zone         = :cq_zone,
    itu_zone        = :itu_zone,
    pfx             = :pfx,
    arrl_sect       = :arrl_sect,
    lat             = :lat,
    lon             = :lon,
    distance        = :distance,
    age             = :age,
    eq_call         = :eq_call,
    station_callsign = :station_callsign,
    operator_call   = :operator_call,
    owner_callsign  = :owner_callsign,
    my_name         = :my_name,
    my_gridsquare   = :my_gridsquare,
    my_city         = :my_city,
    my_state        = :my_state,
    my_county       = :my_county,
    my_country      = :my_country,
    my_dxcc         = :my_dxcc,
    my_cq_zone      = :my_cq_zone,
    my_itu_zone     = :my_itu_zone,
    my_lat          = :my_lat,
    my_lon          = :my_lon,
    tx_pwr          = :tx_pwr,
    rx_pwr          = :rx_pwr,
    antenna         = :antenna,
    ant_az          = :ant_az,
    ant_el          = :ant_el,
    ant_path        = :ant_path,
    rig             = :rig,
    lotw_qsl_sent   = :lotw_qsl_sent,
    lotw_qsl_rcvd   = :lotw_qsl_rcvd,
    lotw_sent_date  = :lotw_sent_date,
    lotw_rcvd_date  = :lotw_rcvd_date,
    eqsl_qsl_sent   = :eqsl_qsl_sent,
    eqsl_qsl_rcvd   = :eqsl_qsl_rcvd,
    eqsl_sent_date  = :eqsl_sent_date,
    eqsl_rcvd_date  = :eqsl_rcvd_date,
    qrz_qsl_sent    = :qrz_qsl_sent,
    qrz_qsl_rcvd    = :qrz_qsl_rcvd,
    qrz_sent_date   = :qrz_sent_date,
    qrz_rcvd_date   = :qrz_rcvd_date,
    clublog_qsl_sent = :clublog_qsl_sent,
    clublog_sent_date = :clublog_sent_date,
    qsl_sent        = :qsl_sent,
    qsl_rcvd        = :qsl_rcvd,
    qsl_sent_date   = :qsl_sent_date,
    qsl_rcvd_date   = :qsl_rcvd_date,
    qsl_via         = :qsl_via,
    qsl_msg         = :qsl_msg,
    prop_mode       = :prop_mode,
    sfi             = :sfi,
    a_index         = :a_index,
    k_index         = :k_index,
    sunspots        = :sunspots,
    contest_id      = :contest_id,
    srx             = :srx,
    srx_string      = :srx_string,
    stx             = :stx,
    stx_string      = :stx_string,
    precedence      = :precedence,
    contest_class   = :contest_class,
    arrl_check      = :arrl_check,
    sig             = :sig,
    sig_info        = :sig_info,
    my_sig          = :my_sig,
    my_sig_info     = :my_sig_info,
    sat_name        = :sat_name,
    sat_mode        = :sat_mode,
    max_bursts      = :max_bursts,
    nr_bursts       = :nr_bursts,
    nr_pings        = :nr_pings,
    ms_shower       = :ms_shower,
    comment         = :comment,
    notes           = :notes,
    qso_complete    = :qso_complete
WHERE id = :id
)";

// ---------------------------------------------------------------------------
// Value binding helpers
// ---------------------------------------------------------------------------

namespace {

// QString → QVariant: empty string stored as NULL
QVariant sv(const QString &s)
{
    return s.isEmpty() ? QVariant() : QVariant(s);
}

// QChar → QVariant (single-char status field)
QVariant cv(QChar c)
{
    return QVariant(QString(c));
}

// QDate → QVariant: invalid date stored as NULL
QVariant dv(const QDate &d)
{
    return d.isValid() ? QVariant(d) : QVariant();
}

// QDateTime → QVariant: invalid datetime stored as NULL
QVariant dtv(const QDateTime &dt)
{
    return dt.isValid() ? QVariant(dt.toUTC()) : QVariant();
}

// std::optional<T> → QVariant
template<typename T>
QVariant ov(const std::optional<T> &opt)
{
    return opt.has_value() ? QVariant(*opt) : QVariant();
}

// int with 0-as-null sentinel → QVariant
QVariant iv0(int v)
{
    return v == 0 ? QVariant() : QVariant(v);
}

// Extract helpers
QChar   qch(const QSqlQuery &q, const char *col)
{
    const QString s = q.value(col).toString();
    return s.isEmpty() ? QChar('N') : s.at(0);
}

QDate   qdate(const QSqlQuery &q, const char *col)
{
    return q.value(col).toDate();
}

QDateTime qdatetime(const QSqlQuery &q, const char *col)
{
    QDateTime dt = q.value(col).toDateTime();
    if (dt.isValid())
        dt = dt.toTimeZone(QTimeZone::utc());
    return dt;
}

template<typename T>
std::optional<T> qopt(const QSqlQuery &q, const char *col)
{
    const QVariant v = q.value(col);
    if (v.isNull()) return std::nullopt;
    return v.value<T>();
}

int qint0(const QSqlQuery &q, const char *col)
{
    const QVariant v = q.value(col);
    return v.isNull() ? 0 : v.toInt();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// SqlBackendBase
// ---------------------------------------------------------------------------

SqlBackendBase::SqlBackendBase()
{
    m_connectionName = QString("NF0T_%1").arg(s_connectionCounter.fetch_add(1));
}

SqlBackendBase::~SqlBackendBase()
{
    close();
    m_db = QSqlDatabase(); // release the handle before removeDatabase()
    QSqlDatabase::removeDatabase(m_connectionName);
}

void SqlBackendBase::close()
{
    if (m_db.isOpen())
        m_db.close();
}

bool SqlBackendBase::isOpen() const
{
    return m_db.isOpen();
}

std::expected<void, QString> SqlBackendBase::execQuery(const QString &sql)
{
    QSqlQuery q(m_db);
    if (!q.exec(sql))
        return std::unexpected(q.lastError().text());
    return {};
}

// ---------------------------------------------------------------------------
// CRUD
// ---------------------------------------------------------------------------

std::expected<qint64, QString> SqlBackendBase::insertQso(Qso &qso)
{
    QSqlQuery q(m_db);
    if (!q.prepare(INSERT_SQL))
        return std::unexpected(q.lastError().text());
    bindQso(q, qso);
    if (!q.exec())
        return std::unexpected(q.lastError().text());
    const qint64 id = q.lastInsertId().toLongLong();
    qso.id = id;
    return id;
}

std::expected<void, QString> SqlBackendBase::updateQso(const Qso &qso)
{
    QSqlQuery q(m_db);
    if (!q.prepare(UPDATE_SQL))
        return std::unexpected(q.lastError().text());
    bindQso(q, qso);
    q.bindValue(":id", qso.id);
    if (!q.exec())
        return std::unexpected(q.lastError().text());
    return {};
}

std::expected<void, QString> SqlBackendBase::deleteQso(qint64 id)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM qsos WHERE id = :id");
    q.bindValue(":id", id);
    if (!q.exec())
        return std::unexpected(q.lastError().text());
    return {};
}

std::expected<QList<Qso>, QString> SqlBackendBase::fetchQsos(const QsoFilter &filter)
{
    // Whitelist order-by columns to prevent SQL injection
    static const QSet<QString> validCols = {
        "datetime_on", "callsign", "band", "mode", "freq",
        "dxcc", "distance", "country", "name"
    };
    const QString orderCol = validCols.contains(filter.orderBy)
                             ? filter.orderBy : QStringLiteral("datetime_on");

    QStringList where;
    QVariantMap binds;

    if (filter.call) {
        where << "callsign LIKE :call";
        binds[":call"] = QString("%%%1%").arg(*filter.call);
    }
    if (filter.band) {
        where << "band = :band";
        binds[":band"] = *filter.band;
    }
    if (filter.mode) {
        where << "mode = :mode";
        binds[":mode"] = *filter.mode;
    }
    if (filter.from) {
        where << "datetime_on >= :from";
        binds[":from"] = filter.from->toUTC();
    }
    if (filter.to) {
        where << "datetime_on <= :to";
        binds[":to"] = filter.to->toUTC();
    }
    if (filter.lotwPending && *filter.lotwPending)
        where << "(lotw_qsl_sent = 'Y' AND lotw_qsl_rcvd != 'Y')";
    if (filter.eqslPending && *filter.eqslPending)
        where << "(eqsl_qsl_sent = 'Y' AND eqsl_qsl_rcvd != 'Y')";
    if (filter.qrzPending && *filter.qrzPending)
        where << "(qrz_qsl_sent = 'Y' AND qrz_qsl_rcvd != 'Y')";
    if (filter.clublogPending && *filter.clublogPending)
        where << "clublog_qsl_sent != 'Y'";

    if (filter.lotwUnsent    && *filter.lotwUnsent)    where << "lotw_qsl_sent    != 'Y'";
    if (filter.eqslUnsent    && *filter.eqslUnsent)    where << "eqsl_qsl_sent    != 'Y'";
    if (filter.qrzUnsent     && *filter.qrzUnsent)     where << "qrz_qsl_sent     != 'Y'";
    if (filter.clublogUnsent && *filter.clublogUnsent) where << "clublog_qsl_sent != 'Y'";

    QString sql = "SELECT * FROM qsos";
    if (!where.isEmpty())
        sql += " WHERE " + where.join(" AND ");
    sql += QString(" ORDER BY %1 %2").arg(orderCol, filter.ascending ? "ASC" : "DESC");
    if (filter.limit > 0)
        sql += QString(" LIMIT %1 OFFSET %2").arg(filter.limit).arg(filter.offset);

    QSqlQuery q(m_db);
    q.prepare(sql);
    for (auto it = binds.cbegin(); it != binds.cend(); ++it)
        q.bindValue(it.key(), it.value());

    if (!q.exec())
        return std::unexpected(q.lastError().text());

    QList<Qso> results;
    while (q.next())
        results.append(qsoFromQuery(q));
    return results;
}

std::expected<int, QString> SqlBackendBase::qsoCount()
{
    QSqlQuery q(m_db);
    if (!q.exec("SELECT COUNT(*) FROM qsos") || !q.next())
        return std::unexpected(q.lastError().text());
    return q.value(0).toInt();
}

// ---------------------------------------------------------------------------
// Bind / Extract
// ---------------------------------------------------------------------------

void SqlBackendBase::bindQso(QSqlQuery &q, const Qso &qso)
{
    q.bindValue(":callsign",         qso.callsign);
    q.bindValue(":datetime_on",      dtv(qso.datetimeOn));
    q.bindValue(":band",             qso.band);
    q.bindValue(":freq",             qso.freq);
    q.bindValue(":mode",             qso.mode);
    q.bindValue(":datetime_off",     dtv(qso.datetimeOff));
    q.bindValue(":submode",          sv(qso.submode));
    q.bindValue(":freq_rx",          ov(qso.freqRx));
    q.bindValue(":band_rx",          sv(qso.bandRx));
    q.bindValue(":rst_sent",         sv(qso.rstSent));
    q.bindValue(":rst_rcvd",         sv(qso.rstRcvd));
    q.bindValue(":name",             sv(qso.name));
    q.bindValue(":contacted_op",     sv(qso.contactedOp));
    q.bindValue(":qth",              sv(qso.qth));
    q.bindValue(":gridsquare",       sv(qso.gridsquare));
    q.bindValue(":country",          sv(qso.country));
    q.bindValue(":state",            sv(qso.state));
    q.bindValue(":county",           sv(qso.county));
    q.bindValue(":cont",             sv(qso.cont));
    q.bindValue(":dxcc",             iv0(qso.dxcc));
    q.bindValue(":cq_zone",          iv0(qso.cqZone));
    q.bindValue(":itu_zone",         iv0(qso.ituZone));
    q.bindValue(":pfx",              sv(qso.pfx));
    q.bindValue(":arrl_sect",        sv(qso.arrlSect));
    q.bindValue(":lat",              ov(qso.lat));
    q.bindValue(":lon",              ov(qso.lon));
    q.bindValue(":distance",         ov(qso.distance));
    q.bindValue(":age",              ov(qso.age));
    q.bindValue(":eq_call",          sv(qso.eqCall));
    q.bindValue(":station_callsign", qso.stationCallsign);
    q.bindValue(":operator_call",    sv(qso.operatorCall));
    q.bindValue(":owner_callsign",   sv(qso.ownerCallsign));
    q.bindValue(":my_name",          sv(qso.myName));
    q.bindValue(":my_gridsquare",    sv(qso.myGridsquare));
    q.bindValue(":my_city",          sv(qso.myCity));
    q.bindValue(":my_state",         sv(qso.myState));
    q.bindValue(":my_county",        sv(qso.myCounty));
    q.bindValue(":my_country",       sv(qso.myCountry));
    q.bindValue(":my_dxcc",          iv0(qso.myDxcc));
    q.bindValue(":my_cq_zone",       iv0(qso.myCqZone));
    q.bindValue(":my_itu_zone",      iv0(qso.myItuZone));
    q.bindValue(":my_lat",           ov(qso.myLat));
    q.bindValue(":my_lon",           ov(qso.myLon));
    q.bindValue(":tx_pwr",           ov(qso.txPwr));
    q.bindValue(":rx_pwr",           ov(qso.rxPwr));
    q.bindValue(":antenna",          sv(qso.antenna));
    q.bindValue(":ant_az",           ov(qso.antAz));
    q.bindValue(":ant_el",           ov(qso.antEl));
    q.bindValue(":ant_path",         sv(qso.antPath));
    q.bindValue(":rig",              sv(qso.rig));
    q.bindValue(":lotw_qsl_sent",    cv(qso.lotwQslSent));
    q.bindValue(":lotw_qsl_rcvd",    cv(qso.lotwQslRcvd));
    q.bindValue(":lotw_sent_date",   dv(qso.lotwSentDate));
    q.bindValue(":lotw_rcvd_date",   dv(qso.lotwRcvdDate));
    q.bindValue(":eqsl_qsl_sent",    cv(qso.eqslQslSent));
    q.bindValue(":eqsl_qsl_rcvd",    cv(qso.eqslQslRcvd));
    q.bindValue(":eqsl_sent_date",   dv(qso.eqslSentDate));
    q.bindValue(":eqsl_rcvd_date",   dv(qso.eqslRcvdDate));
    q.bindValue(":qrz_qsl_sent",     cv(qso.qrzQslSent));
    q.bindValue(":qrz_qsl_rcvd",     cv(qso.qrzQslRcvd));
    q.bindValue(":qrz_sent_date",    dv(qso.qrzSentDate));
    q.bindValue(":qrz_rcvd_date",    dv(qso.qrzRcvdDate));
    q.bindValue(":clublog_qsl_sent", cv(qso.clublogQslSent));
    q.bindValue(":clublog_sent_date",dv(qso.clublogSentDate));
    q.bindValue(":qsl_sent",         cv(qso.qslSent));
    q.bindValue(":qsl_rcvd",         cv(qso.qslRcvd));
    q.bindValue(":qsl_sent_date",    dv(qso.qslSentDate));
    q.bindValue(":qsl_rcvd_date",    dv(qso.qslRcvdDate));
    q.bindValue(":qsl_via",          sv(qso.qslVia));
    q.bindValue(":qsl_msg",          sv(qso.qslMsg));
    q.bindValue(":prop_mode",        sv(qso.propMode));
    q.bindValue(":sfi",              ov(qso.sfi));
    q.bindValue(":a_index",          ov(qso.aIndex));
    q.bindValue(":k_index",          ov(qso.kIndex));
    q.bindValue(":sunspots",         ov(qso.sunspots));
    q.bindValue(":contest_id",       sv(qso.contestId));
    q.bindValue(":srx",              ov(qso.srx));
    q.bindValue(":srx_string",       sv(qso.srxString));
    q.bindValue(":stx",              ov(qso.stx));
    q.bindValue(":stx_string",       sv(qso.stxString));
    q.bindValue(":precedence",       sv(qso.precedence));
    q.bindValue(":contest_class",    sv(qso.contestClass));
    q.bindValue(":arrl_check",       sv(qso.arrlCheck));
    q.bindValue(":sig",              sv(qso.sig));
    q.bindValue(":sig_info",         sv(qso.sigInfo));
    q.bindValue(":my_sig",           sv(qso.mySig));
    q.bindValue(":my_sig_info",      sv(qso.mySigInfo));
    q.bindValue(":sat_name",         sv(qso.satName));
    q.bindValue(":sat_mode",         sv(qso.satMode));
    q.bindValue(":max_bursts",       ov(qso.maxBursts));
    q.bindValue(":nr_bursts",        ov(qso.nrBursts));
    q.bindValue(":nr_pings",         ov(qso.nrPings));
    q.bindValue(":ms_shower",        sv(qso.msShower));
    q.bindValue(":comment",          sv(qso.comment));
    q.bindValue(":notes",            sv(qso.notes));
    q.bindValue(":qso_complete",     sv(qso.qsoComplete));
}

Qso SqlBackendBase::qsoFromQuery(const QSqlQuery &q)
{
    Qso qso;
    qso.id              = q.value("id").toLongLong();
    qso.callsign        = q.value("callsign").toString();
    qso.datetimeOn      = qdatetime(q, "datetime_on");
    qso.band            = q.value("band").toString();
    qso.freq            = q.value("freq").toDouble();
    qso.mode            = q.value("mode").toString();
    qso.datetimeOff     = qdatetime(q, "datetime_off");
    qso.submode         = q.value("submode").toString();
    qso.freqRx          = qopt<double>(q, "freq_rx");
    qso.bandRx          = q.value("band_rx").toString();
    qso.rstSent         = q.value("rst_sent").toString();
    qso.rstRcvd         = q.value("rst_rcvd").toString();
    qso.name            = q.value("name").toString();
    qso.contactedOp     = q.value("contacted_op").toString();
    qso.qth             = q.value("qth").toString();
    qso.gridsquare      = q.value("gridsquare").toString();
    qso.country         = q.value("country").toString();
    qso.state           = q.value("state").toString();
    qso.county          = q.value("county").toString();
    qso.cont            = q.value("cont").toString();
    qso.dxcc            = qint0(q, "dxcc");
    qso.cqZone          = qint0(q, "cq_zone");
    qso.ituZone         = qint0(q, "itu_zone");
    qso.pfx             = q.value("pfx").toString();
    qso.arrlSect        = q.value("arrl_sect").toString();
    qso.lat             = qopt<double>(q, "lat");
    qso.lon             = qopt<double>(q, "lon");
    qso.distance        = qopt<double>(q, "distance");
    qso.age             = qopt<int>(q, "age");
    qso.eqCall          = q.value("eq_call").toString();
    qso.stationCallsign = q.value("station_callsign").toString();
    qso.operatorCall    = q.value("operator_call").toString();
    qso.ownerCallsign   = q.value("owner_callsign").toString();
    qso.myName          = q.value("my_name").toString();
    qso.myGridsquare    = q.value("my_gridsquare").toString();
    qso.myCity          = q.value("my_city").toString();
    qso.myState         = q.value("my_state").toString();
    qso.myCounty        = q.value("my_county").toString();
    qso.myCountry       = q.value("my_country").toString();
    qso.myDxcc          = qint0(q, "my_dxcc");
    qso.myCqZone        = qint0(q, "my_cq_zone");
    qso.myItuZone       = qint0(q, "my_itu_zone");
    qso.myLat           = qopt<double>(q, "my_lat");
    qso.myLon           = qopt<double>(q, "my_lon");
    qso.txPwr           = qopt<double>(q, "tx_pwr");
    qso.rxPwr           = qopt<double>(q, "rx_pwr");
    qso.antenna         = q.value("antenna").toString();
    qso.antAz           = qopt<double>(q, "ant_az");
    qso.antEl           = qopt<double>(q, "ant_el");
    qso.antPath         = q.value("ant_path").toString();
    qso.rig             = q.value("rig").toString();
    qso.lotwQslSent     = qch(q, "lotw_qsl_sent");
    qso.lotwQslRcvd     = qch(q, "lotw_qsl_rcvd");
    qso.lotwSentDate    = qdate(q, "lotw_sent_date");
    qso.lotwRcvdDate    = qdate(q, "lotw_rcvd_date");
    qso.eqslQslSent     = qch(q, "eqsl_qsl_sent");
    qso.eqslQslRcvd     = qch(q, "eqsl_qsl_rcvd");
    qso.eqslSentDate    = qdate(q, "eqsl_sent_date");
    qso.eqslRcvdDate    = qdate(q, "eqsl_rcvd_date");
    qso.qrzQslSent      = qch(q, "qrz_qsl_sent");
    qso.qrzQslRcvd      = qch(q, "qrz_qsl_rcvd");
    qso.qrzSentDate     = qdate(q, "qrz_sent_date");
    qso.qrzRcvdDate     = qdate(q, "qrz_rcvd_date");
    qso.clublogQslSent  = qch(q, "clublog_qsl_sent");
    qso.clublogSentDate = qdate(q, "clublog_sent_date");
    qso.qslSent         = qch(q, "qsl_sent");
    qso.qslRcvd         = qch(q, "qsl_rcvd");
    qso.qslSentDate     = qdate(q, "qsl_sent_date");
    qso.qslRcvdDate     = qdate(q, "qsl_rcvd_date");
    qso.qslVia          = q.value("qsl_via").toString();
    qso.qslMsg          = q.value("qsl_msg").toString();
    qso.propMode        = q.value("prop_mode").toString();
    qso.sfi             = qopt<double>(q, "sfi");
    qso.aIndex          = qopt<double>(q, "a_index");
    qso.kIndex          = qopt<double>(q, "k_index");
    qso.sunspots        = qopt<int>(q, "sunspots");
    qso.contestId       = q.value("contest_id").toString();
    qso.srx             = qopt<int>(q, "srx");
    qso.srxString       = q.value("srx_string").toString();
    qso.stx             = qopt<int>(q, "stx");
    qso.stxString       = q.value("stx_string").toString();
    qso.precedence      = q.value("precedence").toString();
    qso.contestClass    = q.value("contest_class").toString();
    qso.arrlCheck       = q.value("arrl_check").toString();
    qso.sig             = q.value("sig").toString();
    qso.sigInfo         = q.value("sig_info").toString();
    qso.mySig           = q.value("my_sig").toString();
    qso.mySigInfo       = q.value("my_sig_info").toString();
    qso.satName         = q.value("sat_name").toString();
    qso.satMode         = q.value("sat_mode").toString();
    qso.maxBursts       = qopt<double>(q, "max_bursts");
    qso.nrBursts        = qopt<int>(q, "nr_bursts");
    qso.nrPings         = qopt<int>(q, "nr_pings");
    qso.msShower        = q.value("ms_shower").toString();
    qso.comment         = q.value("comment").toString();
    qso.notes           = q.value("notes").toString();
    qso.qsoComplete     = q.value("qso_complete").toString();
    return qso;
}
