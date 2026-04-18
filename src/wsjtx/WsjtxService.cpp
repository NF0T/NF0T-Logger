// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "WsjtxService.h"

#include <QDataStream>
#include <QHostAddress>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QTimeZone>
#include <QUdpSocket>

#include "app/settings/Settings.h"
#include "core/adif/AdifParser.h"

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

WsjtxService::WsjtxService(QObject *parent)
    : DigitalListenerService(parent)
{}

WsjtxService::~WsjtxService()
{
    stop();
}

// ---------------------------------------------------------------------------
// DigitalListenerService interface
// ---------------------------------------------------------------------------

bool WsjtxService::isEnabled() const
{
    return Settings::instance().wsjtxEnabled();
}

bool WsjtxService::isRunning() const
{
    return m_socket && m_socket->state() == QAbstractSocket::BoundState;
}

bool WsjtxService::start()
{
    const Settings &s = Settings::instance();
    if (!startOnAddress(s.wsjtxPort(), s.wsjtxUdpAddress()))
        return false;
    emit started();
    return true;
}

void WsjtxService::stop()
{
    if (!m_socket) return;

    if (!m_multicastGroup.isEmpty()) {
        const QHostAddress grp(m_multicastGroup);
        const QStringList selectedIfaces = Settings::instance().wsjtxMulticastIfaces();
        if (!selectedIfaces.isEmpty()) {
            for (const QString &name : selectedIfaces) {
                const QNetworkInterface iface = QNetworkInterface::interfaceFromName(name);
                if (iface.isValid())
                    m_socket->leaveMulticastGroup(grp, iface);
            }
        } else {
            m_socket->leaveMulticastGroup(grp);
        }
    }

    m_socket->close();
    m_socket->deleteLater();
    m_socket = nullptr;
    emit stopped();
}

// ---------------------------------------------------------------------------
// Private: bind socket
// ---------------------------------------------------------------------------

bool WsjtxService::startOnAddress(quint16 port, const QString &udpAddress)
{
    stop();

    m_port = port;

    const QHostAddress addr(udpAddress.isEmpty() ? QStringLiteral("0.0.0.0") : udpAddress);
    const bool isMulticast = addr.isMulticast();
    m_multicastGroup = isMulticast ? udpAddress : QString();

    m_socket = new QUdpSocket(this);

    if (!m_socket->bind(QHostAddress::AnyIPv4, port,
                        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit logMessage(tr("WSJT-X: failed to bind UDP port %1: %2")
                            .arg(port).arg(m_socket->errorString()));
        m_socket->deleteLater();
        m_socket = nullptr;
        return false;
    }

    if (isMulticast) {
        const QStringList selectedIfaces = Settings::instance().wsjtxMulticastIfaces();
        bool joined = false;

        if (!selectedIfaces.isEmpty()) {
            // Join only the interfaces the user explicitly selected
            for (const QString &name : selectedIfaces) {
                const QNetworkInterface iface = QNetworkInterface::interfaceFromName(name);
                if (!iface.isValid()) continue;
                if (m_socket->joinMulticastGroup(addr, iface)) {
                    joined = true;
                    emit logMessage(tr("WSJT-X: joined multicast %1 on %2.")
                                        .arg(udpAddress, name));
                }
            }
        } else {
            // Auto: join on all multicast-capable interfaces (including loopback)
            const auto ifaces = QNetworkInterface::allInterfaces();
            for (const QNetworkInterface &iface : ifaces) {
                const auto flags = iface.flags();
                if (!(flags & QNetworkInterface::IsUp))         continue;
                if (!(flags & QNetworkInterface::CanMulticast)) continue;
                if (m_socket->joinMulticastGroup(addr, iface))
                    joined = true;
            }
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
    ds.setVersion(QDataStream::Qt_5_0);
    ds.setByteOrder(QDataStream::BigEndian);

    quint32 magic = 0;
    ds >> magic;
    if (magic != MAGIC) return;

    quint32 schema = 0;
    ds >> schema;

    quint32 msgType = 0;
    ds >> msgType;

    const QString clientId = readString(ds);

    switch (msgType) {
    case 0:  handleHeartbeat (ds, clientId); break;
    case 1:  handleStatus    (ds, clientId); break;
    case 3:  handleClear     (ds, clientId); break;
    case 5:  handleQsoLogged (ds, clientId); break;
    case 12: handleLoggedAdif(ds, clientId); break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// Message handlers
// ---------------------------------------------------------------------------

void WsjtxService::handleHeartbeat(QDataStream &ds, const QString &clientId)
{
    quint32 maxSchema = 0;
    ds >> maxSchema;
    const QString version  = readString(ds);
    const QString revision = readString(ds);
    Q_UNUSED(revision)
    Q_UNUSED(clientId)
}

void WsjtxService::handleStatus(QDataStream &ds, const QString &clientId)
{
    Q_UNUSED(clientId)

    quint64 freqHz = 0;
    ds >> freqHz;

    QString mode    = readString(ds);
    QString dxCall  = readString(ds).toUpper();

    const QString report = readString(ds);
    Q_UNUSED(report)

    QString submode = readString(ds);

    quint8 txEnabled = 0, transmitting = 0, decoding = 0;
    ds >> txEnabled >> transmitting >> decoding;

    quint32 rxDF = 0, txDF = 0;
    ds >> rxDF >> txDF;

    const QString deCall = readString(ds).toUpper();
    const QString deGrid = readString(ds).toUpper();
    const QString dxGrid = readString(ds).toUpper();
    Q_UNUSED(deCall) Q_UNUSED(deGrid)

    quint8 txWatchdog = 0;
    ds >> txWatchdog;

    const QString sub = readString(ds);
    if (!sub.isEmpty())
        submode = sub;

    quint8 fastMode = 0;
    ds >> fastMode;

    quint32 specialOpMode = 0, freqTolerance = 0, trPeriod = 0;
    ds >> specialOpMode >> freqTolerance >> trPeriod;

    readString(ds); // configName
    readString(ds); // txMessage

    // Emit radio status (freq + mode) every cycle — receiver guards against
    // overriding Hamlib/TCI when a hardware backend is connected.
    emit radioStatusChanged(freqHz, mode, submode);

    // Emit station change only when the DX call or grid actually changes,
    // so we don't clobber what the operator is typing between Status ticks.
    if (dxCall != m_lastDxCall || dxGrid != m_lastDxGrid) {
        m_lastDxCall = dxCall;
        m_lastDxGrid = dxGrid;
        emit stationSelected(dxCall, dxGrid);
    }
}

void WsjtxService::handleClear(QDataStream &ds, const QString &clientId)
{
    Q_UNUSED(ds) Q_UNUSED(clientId)
    emit cleared();
}

void WsjtxService::handleQsoLogged(QDataStream &ds, const QString &clientId)
{
    Q_UNUSED(clientId)

    const QDateTime timeOff = readDateTime(ds);
    const QString   dxCall  = readString(ds).toUpper();
    const QString   dxGrid  = readString(ds).toUpper();

    quint64 freqHz = 0;
    ds >> freqHz;

    const QString rawMode   = readString(ds);
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

    QString propMode;
    if (!ds.atEnd())
        propMode = readString(ds);

    Qso qso;
    qso.callsign        = dxCall;
    qso.datetimeOn      = timeOn.isValid()  ? timeOn.toUTC()  : QDateTime::currentDateTimeUtc();
    qso.datetimeOff     = timeOff.isValid() ? timeOff.toUTC() : QDateTime();
    qso.freq            = static_cast<double>(freqHz) / 1e6;
    qso.gridsquare      = dxGrid;
    qso.name            = name;
    qso.rstSent         = rstSent;
    qso.rstRcvd         = rstRcvd;
    qso.comment         = comments;
    qso.operatorCall    = opCall;
    qso.stationCallsign = myCall;
    qso.myGridsquare    = myGrid;
    qso.propMode        = propMode;

    if (txPwr.toDouble() > 0)
        qso.txPwr = txPwr.toDouble();

    normaliseMode(rawMode, qso.mode, qso.submode);

    emit qsoLogged(qso);
}

void WsjtxService::handleLoggedAdif(QDataStream &ds, const QString &clientId)
{
    Q_UNUSED(clientId)

    const QString adifText = readString(ds);
    if (adifText.isEmpty()) return;

    const AdifParser::Result parsed = AdifParser::parseString(adifText);
    for (Qso qso : parsed.qsos) {
        QString mode, submode;
        normaliseMode(qso.mode, mode, submode);
        qso.mode    = mode;
        qso.submode = submode;
        emit qsoLogged(qso);
    }
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

void WsjtxService::normaliseMode(const QString &raw, QString &mode, QString &submode)
{
    const QString up = raw.toUpper();
    static const struct { const char *from; const char *toMode; const char *toSub; } kMap[] = {
        {"FT4",   "MFSK", "FT4"},
        {"FST4",  "MFSK", "FST4"},
        {"FST4W", "MFSK", "FST4W"},
        {"JS8",   "MFSK", "JS8"},
        {"Q65",   "MFSK", "Q65"},
    };
    for (const auto &m : kMap) {
        if (up == QLatin1String(m.from)) {
            mode    = QString::fromLatin1(m.toMode);
            submode = QString::fromLatin1(m.toSub);
            return;
        }
    }
    mode    = up;
    submode = {};
}

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
    quint64 julianDay = 0;
    quint32 msOfDay   = 0;
    quint8  timeSpec  = 0;
    ds >> julianDay >> msOfDay >> timeSpec;

    QDate date = QDate::fromJulianDay(static_cast<qint64>(julianDay));
    const int ms = static_cast<int>(qMin(msOfDay, quint32(86'400'000 - 1)));
    QTime time = QTime::fromMSecsSinceStartOfDay(ms);
    return QDateTime(date, time, QTimeZone::utc());
}

bool WsjtxService::readBool(QDataStream &ds)
{
    quint8 v = 0;
    ds >> v;
    return v != 0;
}
