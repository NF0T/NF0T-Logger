#include "TciBackend.h"

#include <QTimer>
#include <QWebSocket>

#include "app/settings/Settings.h"

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

TciBackend::TciBackend(QObject *parent)
    : QObject(parent)
{
    m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    connect(m_socket, &QWebSocket::connected,    this, &TciBackend::onConnected);
    connect(m_socket, &QWebSocket::disconnected, this, &TciBackend::onDisconnected);
    connect(m_socket, &QWebSocket::textMessageReceived,
            this, &TciBackend::onTextMessageReceived);
    connect(m_socket, &QWebSocket::errorOccurred,
            this, &TciBackend::onError);

    // Reconnect timer — fires once after 5 s if the connection drops unexpectedly
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(5000);
    connect(m_reconnectTimer, &QTimer::timeout, this, &TciBackend::onReconnectTimer);
}

TciBackend::~TciBackend()
{
    disconnectTci();
}

// ---------------------------------------------------------------------------
// Connect / disconnect
// ---------------------------------------------------------------------------

bool TciBackend::connectTci()
{
    const Settings &s = Settings::instance();
    const QString host = s.tciHost().isEmpty() ? QStringLiteral("localhost") : s.tciHost();
    const int     port = s.tciPort();

    const QUrl url(QStringLiteral("ws://%1:%2").arg(host).arg(port));

    m_intentionalClose = false;
    m_serverReady      = false;
    m_lastFreqHz       = 0.0;
    m_lastTciMode.clear();

    m_socket->open(url);
    return true;  // actual success reported via connected() / error() signals
}

void TciBackend::disconnectTci()
{
    m_intentionalClose = true;
    m_reconnectTimer->stop();
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->close();
}

bool TciBackend::isConnected() const
{
    return m_connected;
}

// ---------------------------------------------------------------------------
// Outbound commands (logger → rig)
// ---------------------------------------------------------------------------

void TciBackend::setFreq(double freqMhz)
{
    if (!m_connected || !m_serverReady) return;
    const qint64 hz = static_cast<qint64>(freqMhz * 1'000'000.0);
    send(QStringLiteral("vfo:0,0,%1").arg(hz));
}

void TciBackend::setMode(const QString &adifMode, const QString &submode)
{
    if (!m_connected || !m_serverReady) return;
    const QString tciMode = adifToTciMode(adifMode, submode);
    send(QStringLiteral("modulation:0,%1").arg(tciMode));
}

// ---------------------------------------------------------------------------
// Private slots
// ---------------------------------------------------------------------------

void TciBackend::onConnected()
{
    m_connected = true;
    emit connected();
}

void TciBackend::onDisconnected()
{
    const bool wasConnected = m_connected;
    m_connected   = false;
    m_serverReady = false;

    if (wasConnected)
        emit disconnected();

    if (!m_intentionalClose)
        m_reconnectTimer->start();
}

void TciBackend::onTextMessageReceived(const QString &message)
{
    // A single WebSocket frame may contain multiple semicolon-delimited commands
    const QStringList cmds = message.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &cmd : cmds)
        parseMessage(cmd.trimmed());
}

void TciBackend::onError(QAbstractSocket::SocketError /*socketError*/)
{
    emit error(m_socket->errorString());
}

void TciBackend::onReconnectTimer()
{
    if (!m_intentionalClose)
        connectTci();
}

// ---------------------------------------------------------------------------
// Message parser
// ---------------------------------------------------------------------------

void TciBackend::parseMessage(const QString &msg)
{
    if (msg.isEmpty()) return;

    // Split on first colon to get command name and parameter list
    const int colonPos = msg.indexOf(QLatin1Char(':'));
    const QString name   = (colonPos >= 0) ? msg.left(colonPos).toLower()  : msg.toLower();
    const QString params = (colonPos >= 0) ? msg.mid(colonPos + 1)         : QString();

    if (name == QLatin1String("ready")) {
        m_serverReady = true;
        return;
    }

    if (name == QLatin1String("vfo")) {
        // vfo:{trx},{vfo},{freq}
        const QStringList parts = params.split(QLatin1Char(','));
        if (parts.size() < 3) return;
        const int trx = parts[0].toInt();
        const int vfo = parts[1].toInt();
        if (trx != 0 || vfo != 0) return;  // only track TRX 0 / VFO A
        bool ok = false;
        const double hz = parts[2].toDouble(&ok);
        if (!ok || hz <= 0.0) return;
        if (hz != m_lastFreqHz) {
            m_lastFreqHz = hz;
            emit freqChanged(hz / 1'000'000.0);
        }
        return;
    }

    if (name == QLatin1String("modulation")) {
        // modulation:{trx},{mode}
        const QStringList parts = params.split(QLatin1Char(','));
        if (parts.size() < 2) return;
        const int trx = parts[0].toInt();
        if (trx != 0) return;
        const QString tciMode = parts[1].trimmed().toUpper();
        if (tciMode == m_lastTciMode) return;
        m_lastTciMode = tciMode;
        QString submode;
        const QString adif = tciModeToAdif(tciMode, submode);
        emit modeChanged(adif, submode);
        return;
    }
}

void TciBackend::send(const QString &cmd)
{
    m_socket->sendTextMessage(cmd + QLatin1Char(';'));
}

// ---------------------------------------------------------------------------
// Mode conversions
// ---------------------------------------------------------------------------

// static
QString TciBackend::tciModeToAdif(const QString &tciMode, QString &submode)
{
    submode.clear();

    if (tciMode == QLatin1String("USB")) { submode = QStringLiteral("USB"); return QStringLiteral("SSB"); }
    if (tciMode == QLatin1String("LSB")) { submode = QStringLiteral("LSB"); return QStringLiteral("SSB"); }
    if (tciMode == QLatin1String("CW")  || tciMode == QLatin1String("CWU")) return QStringLiteral("CW");
    if (tciMode == QLatin1String("CWL"))  return QStringLiteral("CW");
    if (tciMode == QLatin1String("AM")  || tciMode == QLatin1String("SAM")) return QStringLiteral("AM");
    if (tciMode == QLatin1String("FM")  || tciMode == QLatin1String("NFM") ||
        tciMode == QLatin1String("WFM")) return QStringLiteral("FM");
    if (tciMode == QLatin1String("RTTY") || tciMode == QLatin1String("RTTYR")) return QStringLiteral("RTTY");
    // DIGU / DIGL — digital modes on USB/LSB sideband; default to SSB
    if (tciMode == QLatin1String("DIGU")) { submode = QStringLiteral("USB"); return QStringLiteral("SSB"); }
    if (tciMode == QLatin1String("DIGL")) { submode = QStringLiteral("LSB"); return QStringLiteral("SSB"); }

    // Unknown — return as-is so the user can see what the rig reported
    return tciMode;
}

// static
QString TciBackend::adifToTciMode(const QString &adifMode, const QString &submode)
{
    if (adifMode == QLatin1String("SSB")) {
        return (submode == QLatin1String("LSB")) ? QStringLiteral("LSB") : QStringLiteral("USB");
    }
    if (adifMode == QLatin1String("CW"))   return QStringLiteral("CW");
    if (adifMode == QLatin1String("AM"))   return QStringLiteral("AM");
    if (adifMode == QLatin1String("FM"))   return QStringLiteral("FM");
    if (adifMode == QLatin1String("RTTY")) return QStringLiteral("RTTY");
    if (adifMode == QLatin1String("FT8")  || adifMode == QLatin1String("FT4") ||
        adifMode == QLatin1String("JS8")  || adifMode == QLatin1String("WSPR") ||
        adifMode == QLatin1String("JT65") || adifMode == QLatin1String("JT9") ||
        adifMode == QLatin1String("MSK144")) {
        // Digital modes run on USB in ExpertSDR
        return QStringLiteral("DIGU");
    }
    // Default
    return QStringLiteral("USB");
}
