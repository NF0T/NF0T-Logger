#include "WsjtxService.h"

#include <QDataStream>
#include <QHostAddress>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QTimeZone>
#include <QUdpSocket>

#include "core/adif/AdifParser.h"

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

WsjtxService::WsjtxService(QObject *parent)
    : QObject(parent)
{}

WsjtxService::~WsjtxService()
{
    stop();
}

// ---------------------------------------------------------------------------
// Start / stop
// ---------------------------------------------------------------------------

bool WsjtxService::start(quint16 port, const QString &udpAddress)
{
    stop();

    m_port = port;

    const QHostAddress addr(udpAddress.isEmpty() ? QStringLiteral("0.0.0.0") : udpAddress);

    // Multicast addresses are in the 224.0.0.0/4 range
    const bool isMulticast = addr.isMulticast();

    m_multicastGroup = isMulticast ? udpAddress : QString();

    m_socket = new QUdpSocket(this);

    // ShareAddress + ReuseAddressHint allows multiple apps (us, GridTracker2,
    // JTAlert, etc.) to all bind the same port and receive every datagram.
    if (!m_socket->bind(QHostAddress::AnyIPv4, port,
                        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit logMessage(tr("WSJT-X: failed to bind UDP port %1: %2")
                            .arg(port).arg(m_socket->errorString()));
        m_socket->deleteLater();
        m_socket = nullptr;
        return false;
    }

    if (isMulticast) {
        // Join on all suitable interfaces so we share the stream with
        // GridTracker2, JTAlert, etc.
        bool joined = false;
        const auto ifaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface &iface : ifaces) {
            const auto flags = iface.flags();
            if (!(flags & QNetworkInterface::IsUp)) continue;
            if (!(flags & QNetworkInterface::CanMulticast)) continue;
            if (flags & QNetworkInterface::IsLoopBack) continue;
            if (m_socket->joinMulticastGroup(addr, iface))
                joined = true;
        }
        if (!joined)
            m_socket->joinMulticastGroup(addr);
    }

    connect(m_socket, &QUdpSocket::readyRead, this, &WsjtxService::onDatagramReady);

    const QString addrDesc = udpAddress.isEmpty()
        ? tr("any interface (unicast)")
        : isMulticast
            ? tr("multicast %1").arg(udpAddress)
            : tr("unicast %1").arg(udpAddress);

    emit logMessage(tr("WSJT-X listener started on UDP port %1 (%2).")
                        .arg(port).arg(addrDesc));
    return true;
}

void WsjtxService::stop()
{
    if (!m_socket) return;

    if (!m_multicastGroup.isEmpty())
        m_socket->leaveMulticastGroup(QHostAddress(m_multicastGroup));

    m_socket->close();
    m_socket->deleteLater();
    m_socket = nullptr;
}

bool WsjtxService::isRunning() const
{
    return m_socket && m_socket->state() == QAbstractSocket::BoundState;
}

// ---------------------------------------------------------------------------
// Receive
// ---------------------------------------------------------------------------

void WsjtxService::onDatagramReady()
{
    while (m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram dg = m_socket->receiveDatagram();
        if (!dg.isNull())
            processPacket(dg.data());
    }
}

void WsjtxService::processPacket(const QByteArray &data)
{
    QDataStream ds(data);
    ds.setVersion(QDataStream::Qt_5_0);   // WSJT-X uses Qt 5 stream format
    ds.setByteOrder(QDataStream::BigEndian);

    quint32 magic = 0;
    ds >> magic;
    if (magic != MAGIC) return;   // not a WSJT-X packet

    quint32 schema = 0;
    ds >> schema;
    // Accept schema 2 and above; we read only the fields defined in schema 2

    quint32 msgType = 0;
    ds >> msgType;

    const QString clientId = readString(ds);

    switch (msgType) {
    case 0:  handleHeartbeat (ds, clientId); break;
    case 1:  handleStatus    (ds, clientId); break;
    case 5:  handleQsoLogged (ds, clientId); break;
    case 12: handleLoggedAdif(ds, clientId); break;
    default: break;   // ignore Decode, Clear, Reply, etc.
    }
}

// ---------------------------------------------------------------------------
// Message handlers
// ---------------------------------------------------------------------------

void WsjtxService::handleHeartbeat(QDataStream &ds, const QString &clientId)
{
    quint32 maxSchema = 0;
    ds >> maxSchema;
    const QString version = readString(ds);
    const QString revision = readString(ds);
    Q_UNUSED(revision)
    emit heartbeat(clientId, version, static_cast<int>(maxSchema));
}

void WsjtxService::handleStatus(QDataStream &ds, const QString &clientId)
{
    Status s;
    s.clientId = clientId;

    quint64 freqHz = 0;
    ds >> freqHz;
    s.dialFreqHz = freqHz;

    s.mode       = readString(ds);
    s.dxCall     = readString(ds).toUpper();

    const QString report = readString(ds);  // RST report
    Q_UNUSED(report)

    s.submode    = readString(ds);   // TX mode in older schema, submode in newer

    quint8 txEnabled = 0, transmitting = 0, decoding = 0;
    ds >> txEnabled >> transmitting >> decoding;
    s.transmitting = transmitting;
    s.decoding     = decoding;

    quint32 rxDF = 0, txDF = 0;
    ds >> rxDF >> txDF;
    s.rxDF = static_cast<qint32>(rxDF);
    s.txDF = static_cast<qint32>(txDF);

    s.deCall  = readString(ds).toUpper();
    s.deGrid  = readString(ds).toUpper();
    s.dxGrid  = readString(ds).toUpper();

    quint8 txWatchdog = 0;
    ds >> txWatchdog;

    // sub-mode (schema 2+)
    const QString sub = readString(ds);
    if (!sub.isEmpty())
        s.submode = sub;

    quint8 fastMode = 0;
    ds >> fastMode;

    // schema 2 extras
    quint32 specialOpMode = 0;
    ds >> specialOpMode;
    quint32 freqTolerance = 0;
    ds >> freqTolerance;
    quint32 trPeriod = 0;
    ds >> trPeriod;

    s.configName = readString(ds);
    s.txMessage  = readString(ds);

    emit statusReceived(s);
}

void WsjtxService::handleQsoLogged(QDataStream &ds, const QString &clientId)
{
    Q_UNUSED(clientId)

    const QDateTime timeOff = readDateTime(ds);
    const QString   dxCall  = readString(ds).toUpper();
    const QString   dxGrid  = readString(ds).toUpper();

    quint64 freqHz = 0;
    ds >> freqHz;

    const QString mode      = readString(ds);
    const QString rstSent   = readString(ds);
    const QString rstRcvd   = readString(ds);
    const QString txPwr     = readString(ds);
    const QString comments  = readString(ds);
    const QString name      = readString(ds);
    const QDateTime timeOn  = readDateTime(ds);
    const QString opCall    = readString(ds);
    const QString myCall    = readString(ds).toUpper();
    const QString myGrid    = readString(ds).toUpper();
    const QString exchSent  = readString(ds);
    const QString exchRcvd  = readString(ds);

    // Schema 3+ adds ADIF prop mode; read if present
    QString propMode;
    if (!ds.atEnd())
        propMode = readString(ds);

    Qso qso;
    qso.callsign    = dxCall;
    qso.datetimeOn  = timeOn.isValid()  ? timeOn.toUTC()  : QDateTime::currentDateTimeUtc();
    qso.datetimeOff = timeOff.isValid() ? timeOff.toUTC() : QDateTime();
    qso.freq        = static_cast<double>(freqHz) / 1e6;
    qso.band        = {};   // will be derived from freq by MainWindow/BandPlan
    qso.gridsquare  = dxGrid;
    qso.name        = name;
    qso.rstSent     = rstSent;
    qso.rstRcvd     = rstRcvd;
    qso.comment     = comments;
    qso.operatorCall = opCall;
    qso.stationCallsign = myCall;
    qso.myGridsquare    = myGrid;
    qso.propMode        = propMode;

    if (txPwr.toDouble() > 0)
        qso.txPwr = txPwr.toDouble();

    // Normalise mode — WSJT-X sends the display name (FT8, FT4, etc.)
    // Apply the same normalisation used in QslDownloadDialog so the stored
    // mode is ADIF-spec compliant.
    const QString mUp = mode.toUpper();
    static const struct { const char *from; const char *toMode; const char *toSub; } kModeMap[] = {
        {"FT4",   "MFSK", "FT4"},
        {"FST4",  "MFSK", "FST4"},
        {"FST4W", "MFSK", "FST4W"},
        {"JS8",   "MFSK", "JS8"},
        {"Q65",   "MFSK", "Q65"},
    };
    bool mapped = false;
    for (const auto &m : kModeMap) {
        if (mUp == QLatin1String(m.from)) {
            qso.mode    = QString::fromLatin1(m.toMode);
            qso.submode = QString::fromLatin1(m.toSub);
            mapped = true;
            break;
        }
    }
    if (!mapped)
        qso.mode = mUp;

    emit qsoLogged(qso);
}

void WsjtxService::handleLoggedAdif(QDataStream &ds, const QString &clientId)
{
    Q_UNUSED(clientId)

    const QString adifText = readString(ds);
    if (adifText.isEmpty()) return;

    const AdifParser::Result parsed = AdifParser::parseString(adifText);
    for (Qso qso : parsed.qsos) {
        // Apply same mode normalisation as QSOLogged handler
        const QString mUp = qso.mode.toUpper();
        static const struct { const char *from; const char *toMode; const char *toSub; } kModeMap[] = {
            {"FT4",   "MFSK", "FT4"},
            {"FST4",  "MFSK", "FST4"},
            {"FST4W", "MFSK", "FST4W"},
            {"JS8",   "MFSK", "JS8"},
            {"Q65",   "MFSK", "Q65"},
        };
        bool mapped = false;
        for (const auto &m : kModeMap) {
            if (mUp == QLatin1String(m.from)) {
                qso.mode    = QString::fromLatin1(m.toMode);
                qso.submode = QString::fromLatin1(m.toSub);
                mapped = true;
                break;
            }
        }
        if (!mapped)
            qso.mode = mUp;

        emit qsoLogged(qso);
    }
}

// ---------------------------------------------------------------------------
// Stream helpers
// ---------------------------------------------------------------------------

QString WsjtxService::readString(QDataStream &ds)
{
    quint32 len = 0;
    ds >> len;
    if (len == 0xFFFFFFFF || len == 0)
        return {};
    const QByteArray bytes = ds.device()->read(len);
    return QString::fromUtf8(bytes);
}

QDateTime WsjtxService::readDateTime(QDataStream &ds)
{
    // Qt QDataStream DateTime: quint64 Julian day, quint32 ms into day, quint8 timespec
    quint64 julianDay = 0;
    quint32 msOfDay   = 0;
    quint8  timeSpec  = 0;
    ds >> julianDay >> msOfDay >> timeSpec;

    QDate date = QDate::fromJulianDay(static_cast<qint64>(julianDay));
    QTime time = QTime::fromMSecsSinceStartOfDay(static_cast<int>(msOfDay));
    QDateTime dt(date, time, QTimeZone::utc());
    return dt;
}

bool WsjtxService::readBool(QDataStream &ds)
{
    quint8 v = 0;
    ds >> v;
    return v != 0;
}
