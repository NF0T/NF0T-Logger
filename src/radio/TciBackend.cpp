// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "TciBackend.h"

#include <cmath>
#include <QSet>
#include <QTimer>
#include <QWebSocket>

#include "app/settings/Settings.h"

namespace {
// Window for treating CLICKED_ON_SPOT + RX_CLICKED_ON_SPOT as one click.
constexpr qint64 kSpotDedupMs = 1000;
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

TciBackend::TciBackend(QObject *parent)
    : RadioBackend(parent)
{
    m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    m_sender = [this](const QString &cmd) { m_socket->sendTextMessage(cmd); };

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
    disconnectRadio();
}

// ---------------------------------------------------------------------------
// Connect / disconnect
// ---------------------------------------------------------------------------

void TciBackend::connectRadio()
{
    const Settings &s = Settings::instance();
    const QString host = s.tciHost().isEmpty() ? QStringLiteral("localhost") : s.tciHost();
    const int     port = s.tciPort();

    const QUrl url(QStringLiteral("ws://%1:%2").arg(host).arg(port));

    m_intentionalClose = false;
    resetSessionState();

    m_socket->open(url);  // success reported via connected() / error() signals
}

void TciBackend::disconnectRadio()
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

bool TciBackend::isSplit() const
{
    return m_trx.value(m_activeTrx).split;
}

void TciBackend::resetSessionState()
{
    // The server re-sends its full state after every connect, so nothing from a
    // previous session may survive into the next one.
    m_serverReady   = false;
    m_transmitting  = false;
    m_deviceName.clear();
    m_protocolVersion.clear();
    m_supportedModulations.clear();
    m_trxCount      = 1;
    m_channelsCount = 1;
    m_receiveOnly   = false;
    m_activeTrx     = 0;
    m_trx.clear();
    m_lastSpotCall.clear();
    m_lastSpotHz    = 0.0;
    m_lastSpotTimer.invalidate();
}

// ---------------------------------------------------------------------------
// Outbound commands (logger → rig)
// ---------------------------------------------------------------------------

void TciBackend::setFreq(double freqMhz)
{
    if (!m_connected || !m_serverReady) return;
    // §4.7: frequencies are integers, and a value that does not parse turns a
    // set into a read — so never send zero, negative or fractional Hz.
    const qint64 hz = static_cast<qint64>(std::llround(freqMhz * 1'000'000.0));
    if (hz <= 0) return;
    send(QStringLiteral("vfo:%1,0,%2").arg(m_activeTrx).arg(hz));
}

void TciBackend::setMode(const QString &adifMode, const QString &submode)
{
    if (!m_connected || !m_serverReady) return;
    const QString tciMode = negotiateModulation(adifToTciMode(adifMode, submode));
    if (tciMode.isEmpty()) return;
    send(QStringLiteral("modulation:%1,%2").arg(m_activeTrx).arg(tciMode));
}

QString TciBackend::negotiateModulation(const QString &tciMode) const
{
    // MODULATIONS_LIST is the server's contract; a mode outside it would be
    // ignored. Fall back to the nearest listed mode, or send nothing.
    if (m_supportedModulations.isEmpty() || m_supportedModulations.contains(tciMode))
        return tciMode;

    static const QMap<QString, QString> fallback = {
        {QStringLiteral("DIGU"), QStringLiteral("USB")},
        {QStringLiteral("DIGL"), QStringLiteral("LSB")},
        {QStringLiteral("CWR"),  QStringLiteral("CW")},
        {QStringLiteral("NFM"),  QStringLiteral("FM")},
        {QStringLiteral("FM"),   QStringLiteral("NFM")},
    };
    const QString alt = fallback.value(tciMode);
    if (!alt.isEmpty() && m_supportedModulations.contains(alt))
        return alt;
    return {};
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

    // A dropped link must not leave the TX indicator lit, or a stale split TX
    // frequency waiting to be logged.
    if (m_transmitting) {
        m_transmitting = false;
        emit transmitChanged(false);
    }
    if (isSplit()) {
        m_trx[m_activeTrx].split = false;
        emit splitChanged(false);
    }

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
        connectRadio();
}

// ---------------------------------------------------------------------------
// Message parser
// ---------------------------------------------------------------------------

void TciBackend::parseMessage(const QString &msg)
{
    if (msg.isEmpty()) return;

    // §3.1: the case of letters does not matter.
    const int colonPos = msg.indexOf(QLatin1Char(':'));
    const QString name   = (colonPos >= 0) ? msg.left(colonPos).trimmed().toLower()
                                           : msg.toLower();
    const QString params = (colonPos >= 0) ? msg.mid(colonPos + 1) : QString();
    const QStringList args = splitArgs(params);

    // Integer argument helper: a field that does not parse makes the whole
    // command invalid, and §4.7 says an invalid command is ignored.
    auto intArg = [&args](qsizetype i, int &out) {
        if (i >= args.size()) return false;
        bool ok = false;
        out = args.at(i).toInt(&ok);
        return ok && out >= 0;
    };
    auto hzArg = [&args](qsizetype i, double &out) {
        if (i >= args.size()) return false;
        bool ok = false;
        out = args.at(i).toDouble(&ok);
        return ok && out > 0.0;
    };

    // --- Initialization commands (§4.1) -----------------------------------

    if (name == QLatin1String("ready")) {
        m_serverReady = true;
        return;
    }

    if (name == QLatin1String("protocol")) {
        m_protocolVersion = args.join(QLatin1Char(','));
        return;
    }

    if (name == QLatin1String("device")) {
        if (args.isEmpty()) return;
        m_deviceName = args.join(QLatin1Char(','));
        emit deviceDiscovered(m_deviceName);
        return;
    }

    if (name == QLatin1String("receive_only")) {
        if (!args.isEmpty()) m_receiveOnly = parseBool(args.at(0));
        return;
    }

    if (name == QLatin1String("trx_count")) {
        int cnt = 0;
        if (intArg(0, cnt) && cnt >= 1) m_trxCount = cnt;
        return;
    }

    // CHANNEL_COUNT is the pre-2.1 name; 2.1 servers no longer send it, older
    // ones still do.
    if (name == QLatin1String("channels_count") || name == QLatin1String("channel_count")) {
        int cnt = 0;
        if (intArg(0, cnt) && cnt >= 1) m_channelsCount = cnt;
        return;
    }

    if (name == QLatin1String("modulations_list")) {
        m_supportedModulations.clear();
        for (const QString &m : args)
            m_supportedModulations.append(m.toUpper());
        return;
    }

    // --- Transceiver focus ------------------------------------------------

    // ECODER_SWITCH_RX:<panel>,<rx> — the receiver the operator's E-Coder
    // panel is driving. Only the first panel defines focus.
    if (name == QLatin1String("ecoder_switch_rx")) {
        int panel = 0, trx = 0;
        if (intArg(0, panel) && intArg(1, trx) && panel == 0)
            switchActiveTrx(trx);
        return;
    }

    // AetherSDR extension: active_slice:<trx>,<letter> on GUI slice focus.
    if (name == QLatin1String("active_slice")) {
        int trx = 0;
        if (intArg(0, trx)) switchActiveTrx(trx);
        return;
    }

    if (name == QLatin1String("rx_enable")) {
        int trx = 0;
        if (!intArg(0, trx) || args.size() < 2) return;
        // §4.5: the first transceiver is always enabled.
        m_trx[trx].enabled = (trx == 0) || parseBool(args.at(1));
        // AetherSDR sends TRX_COUNT only at connect and announces slices
        // opened later with RX_ENABLE, so an enabled receiver widens the range.
        if (m_trx[trx].enabled && trx >= m_trxCount)
            m_trxCount = trx + 1;
        if (!m_trx[trx].enabled && trx == m_activeTrx)
            switchActiveTrx(0);
        return;
    }

    // --- Per-transceiver state (§4.2) -------------------------------------

    if (name == QLatin1String("vfo")) {
        // VFO:<trx>,<channel>,<Hz>; the two-argument form is a read request.
        int trx = 0, channel = 0;
        double hz = 0.0;
        if (!intArg(0, trx) || !intArg(1, channel) || !hzArg(2, hz)) return;

        TrxState &st = m_trx[trx];
        if (channel == 0) {
            st.vfoAHz = hz;
            if (trx == m_activeTrx)
                emit freqChanged(hz / 1'000'000.0);
        } else if (channel == 1) {
            // Channel B is a second receive channel; it is the TX frequency
            // only while split is enabled.
            st.vfoBHz = hz;
            if (trx == m_activeTrx && st.split)
                emit txFreqChanged(hz / 1'000'000.0);
        }
        return;
    }

    if (name == QLatin1String("split_enable")) {
        int trx = 0;
        if (!intArg(0, trx) || args.size() < 2) return;
        TrxState &st = m_trx[trx];
        st.split = parseBool(args.at(1));
        if (trx == m_activeTrx) {
            emit splitChanged(st.split);
            if (st.split && st.vfoBHz > 0.0)
                emit txFreqChanged(st.vfoBHz / 1'000'000.0);
        }
        return;
    }

    if (name == QLatin1String("modulation")) {
        int trx = 0;
        if (!intArg(0, trx) || args.size() < 2) return;
        const QString tciMode = args.at(1).toUpper();
        m_trx[trx].tciMode = tciMode;
        if (trx == m_activeTrx) {
            QString submode;
            const QString adif = tciModeToAdif(tciMode, submode);
            emit modeChanged(adif, submode);
        }
        return;
    }

    if (name == QLatin1String("trx")) {
        // TRX:<trx>,<state>[,<source>]; the one-argument form is a read request.
        int trx = 0;
        if (!intArg(0, trx) || args.size() < 2) return;
        m_trx[trx].tx = parseBool(args.at(1));
        updateTransmitting();
        return;
    }

    // --- Notifications (§4.4) ---------------------------------------------

    if (name == QLatin1String("tx_frequency")) {
        // Not transceiver-qualified: it is whatever the radio transmits on.
        // Used only while split, where it supersedes the VFO B estimate.
        double hz = 0.0;
        if (hzArg(0, hz) && isSplit())
            emit txFreqChanged(hz / 1'000'000.0);
        return;
    }

    if (name == QLatin1String("rx_clicked_on_spot")) {
        // RX_CLICKED_ON_SPOT:<trx>,<channel>,<callsign>,<Hz>
        int trx = 0, channel = 0;
        double hz = 0.0;
        if (intArg(0, trx) && intArg(1, channel) && args.size() >= 4 && hzArg(3, hz))
            emitSpotClicked(args.at(2), hz);
        return;
    }

    if (name == QLatin1String("clicked_on_spot")) {
        // Legacy form, channel A only: CLICKED_ON_SPOT:<callsign>,<Hz>
        double hz = 0.0;
        if (args.size() >= 2 && hzArg(1, hz))
            emitSpotClicked(args.at(0), hz);
        return;
    }
}

void TciBackend::switchActiveTrx(int trx)
{
    if (!trxInRange(trx) || trx == m_activeTrx) return;
    if (!m_trx.value(trx).enabled) return;
    m_activeTrx = trx;

    const TrxState st = m_trx.value(m_activeTrx);
    if (st.vfoAHz > 0.0)
        emit freqChanged(st.vfoAHz / 1'000'000.0);
    if (!st.tciMode.isEmpty()) {
        QString submode;
        const QString adif = tciModeToAdif(st.tciMode, submode);
        emit modeChanged(adif, submode);
    }
    emit splitChanged(st.split);
    if (st.split && st.vfoBHz > 0.0)
        emit txFreqChanged(st.vfoBHz / 1'000'000.0);
}

void TciBackend::updateTransmitting()
{
    // The TX indicator means "the radio is transmitting", whichever
    // transceiver keyed — AetherSDR reports TRX against the TX slice, which
    // need not be the focused one.
    bool any = false;
    for (const TrxState &st : std::as_const(m_trx))
        any = any || st.tx;
    if (any != m_transmitting) {
        m_transmitting = any;
        emit transmitChanged(any);
    }
}

void TciBackend::emitSpotClicked(const QString &callsign, double freqHz)
{
    const QString call = callsign.toUpper();
    if (call.isEmpty()) return;
    if (m_lastSpotTimer.isValid() && m_lastSpotTimer.elapsed() < kSpotDedupMs
        && call == m_lastSpotCall && std::abs(freqHz - m_lastSpotHz) < 1.0) {
        return;
    }
    m_lastSpotCall = call;
    m_lastSpotHz   = freqHz;
    m_lastSpotTimer.start();
    emit spotClicked(call, freqHz / 1'000'000.0);
}

bool TciBackend::trxInRange(int trx) const
{
    // §4.7: a receiver number out of range is ignored by the server.
    return trx >= 0 && trx < m_trxCount;
}

void TciBackend::send(const QString &cmd)
{
    m_sender(cmd + QLatin1Char(';'));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// static
QStringList TciBackend::splitArgs(const QString &params)
{
    // §4.7: empty arguments are discarded during parsing.
    QStringList out;
    const QStringList raw = params.split(QLatin1Char(','));
    for (const QString &a : raw) {
        const QString t = a.trimmed();
        if (!t.isEmpty()) out.append(t);
    }
    return out;
}

// static
bool TciBackend::parseBool(const QString &str)
{
    // §4.7: true if it equals "true" or "1" in any case; anything else is false.
    const QString s = str.trimmed().toLower();
    return s == QLatin1String("true") || s == QLatin1String("1");
}

// static
QString TciBackend::tciModeToAdif(const QString &tciMode, QString &submode)
{
    submode.clear();

    if (tciMode == QLatin1String("USB")) { submode = QStringLiteral("USB"); return QStringLiteral("SSB"); }
    if (tciMode == QLatin1String("LSB")) { submode = QStringLiteral("LSB"); return QStringLiteral("SSB"); }
    if (tciMode == QLatin1String("DSB")) return QStringLiteral("SSB");   // ADIF has no DSB submode
    if (tciMode == QLatin1String("CW")  || tciMode == QLatin1String("CWU") ||
        tciMode == QLatin1String("CWL") || tciMode == QLatin1String("CWR")) return QStringLiteral("CW");
    if (tciMode == QLatin1String("AM")  || tciMode == QLatin1String("SAM")) return QStringLiteral("AM");
    if (tciMode == QLatin1String("FM")  || tciMode == QLatin1String("NFM") ||
        tciMode == QLatin1String("WFM")) return QStringLiteral("FM");
    if (tciMode == QLatin1String("RTTY") || tciMode == QLatin1String("RTTYR")) return QStringLiteral("RTTY");
    // DIGU/DIGL say only "data on a sideband", not which digital mode; ADIF
    // has no generic data mode. The digital-mode app (WSJT-X etc.) supplies
    // the real mode when it logs.
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

    static const QSet<QString> sidebandData = {
        QStringLiteral("FT8"),    QStringLiteral("FT4"),     QStringLiteral("MFSK"),
        QStringLiteral("JS8"),    QStringLiteral("FST4"),    QStringLiteral("WSPR"),
        QStringLiteral("JT65"),   QStringLiteral("JT9"),     QStringLiteral("MSK144"),
        QStringLiteral("Q65"),    QStringLiteral("PSK"),     QStringLiteral("OLIVIA"),
        QStringLiteral("CONTESTI"), QStringLiteral("THOR"),  QStringLiteral("DOMINO"),
        QStringLiteral("MT63"),   QStringLiteral("HELL"),    QStringLiteral("PKT"),
        QStringLiteral("DYNAMIC"), QStringLiteral("ARDOP"),
    };
    if (sidebandData.contains(adifMode))
        return (submode == QLatin1String("LSB")) ? QStringLiteral("DIGL") : QStringLiteral("DIGU");

    // Default
    return QStringLiteral("USB");
}
