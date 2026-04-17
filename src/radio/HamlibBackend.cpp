#include "HamlibBackend.h"

#include <QTimer>

#include "app/settings/Settings.h"

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

HamlibBackend::HamlibBackend(QObject *parent)
    : RadioBackend(parent)
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(500);
    connect(m_pollTimer, &QTimer::timeout, this, &HamlibBackend::poll);
}

HamlibBackend::~HamlibBackend()
{
    disconnectRadio();
}

// ---------------------------------------------------------------------------
// Connect / disconnect
// ---------------------------------------------------------------------------

bool HamlibBackend::connectRadio()
{
#ifndef HAVE_HAMLIB
    emit error(tr("Hamlib support was not compiled in."));
    return false;
#else
    if (m_connected)
        disconnectRadio();

    const Settings &s = Settings::instance();
    const int model   = s.hamlibRigModel();

    m_rig = rig_init(static_cast<rig_model_t>(model));
    if (!m_rig) {
        emit error(tr("rig_init failed for model %1. Check rig model number.").arg(model));
        return false;
    }

    // Short timeout so the poll never hangs the UI for long
    m_rig->state.rigport.timeout = 1000;
    m_rig->state.rigport.retry   = 1;

    bool ok = false;
    if (s.hamlibConnectionType() == QLatin1String("network")) {
        ok = configureNetwork();
    } else {
        ok = configureSerial();
    }

    if (!ok) {
        rig_cleanup(m_rig);
        m_rig = nullptr;
        return false;
    }

    const int ret = rig_open(m_rig);
    if (ret != RIG_OK) {
        emit error(tr("rig_open failed: %1").arg(QString::fromLatin1(rigerror(ret))));
        rig_cleanup(m_rig);
        m_rig = nullptr;
        return false;
    }

    m_connected  = true;
    m_lastFreqHz = 0.0;
    m_lastMode   = RIG_MODE_NONE;
    m_pollTimer->start();
    emit connected();
    return true;
#endif
}

void HamlibBackend::disconnectRadio()
{
#ifdef HAVE_HAMLIB
    m_pollTimer->stop();
    if (m_rig) {
        if (m_connected)
            rig_close(m_rig);
        rig_cleanup(m_rig);
        m_rig = nullptr;
    }
#endif
    if (m_connected) {
        m_connected = false;
        emit disconnected();
    }
}

bool HamlibBackend::isConnected() const
{
    return m_connected;
}

// ---------------------------------------------------------------------------
// Outbound commands (logger → rig)
// ---------------------------------------------------------------------------

void HamlibBackend::setFreq(double freqMhz)
{
#ifdef HAVE_HAMLIB
    if (!m_connected || !m_rig) return;
    const freq_t hz = freqMhz * 1'000'000.0;
    rig_set_freq(m_rig, RIG_VFO_CURR, hz);
#else
    Q_UNUSED(freqMhz)
#endif
}

void HamlibBackend::setMode(const QString &adifMode, const QString &submode)
{
#ifdef HAVE_HAMLIB
    if (!m_connected || !m_rig) return;
    const rmode_t mode  = adifToRigMode(adifMode, submode);
    const pbwidth_t bw  = rig_passband_normal(m_rig, mode);
    rig_set_mode(m_rig, RIG_VFO_CURR, mode, bw);
#else
    Q_UNUSED(adifMode)
    Q_UNUSED(submode)
#endif
}

// ---------------------------------------------------------------------------
// Poll (rig → logger)
// ---------------------------------------------------------------------------

void HamlibBackend::poll()
{
#ifdef HAVE_HAMLIB
    if (!m_connected || !m_rig) return;
    readFreq();
    readMode();
    readPtt();
#endif
}

// ---------------------------------------------------------------------------
// Private Hamlib helpers
// ---------------------------------------------------------------------------

#ifdef HAVE_HAMLIB

bool HamlibBackend::configureSerial()
{
    const Settings &s = Settings::instance();
    const QByteArray device = s.hamlibSerialDevice().toLocal8Bit();

    m_rig->state.rigport.type.rig = RIG_PORT_SERIAL;
    qstrncpy(m_rig->state.rigport.pathname, device.constData(), FILPATHLEN - 1);

    m_rig->state.rigport.parm.serial.rate      = s.hamlibBaudRate();
    m_rig->state.rigport.parm.serial.data_bits = s.hamlibDataBits();
    m_rig->state.rigport.parm.serial.stop_bits = s.hamlibStopBits();

    const QString parity = s.hamlibParity();
    if (parity == QLatin1String("Even"))
        m_rig->state.rigport.parm.serial.parity = RIG_PARITY_EVEN;
    else if (parity == QLatin1String("Odd"))
        m_rig->state.rigport.parm.serial.parity = RIG_PARITY_ODD;
    else
        m_rig->state.rigport.parm.serial.parity = RIG_PARITY_NONE;

    const QString flow = s.hamlibFlowControl();
    if (flow == QLatin1String("Hardware"))
        m_rig->state.rigport.parm.serial.handshake = RIG_HANDSHAKE_HARDWARE;
    else if (flow == QLatin1String("Software"))
        m_rig->state.rigport.parm.serial.handshake = RIG_HANDSHAKE_XONXOFF;
    else
        m_rig->state.rigport.parm.serial.handshake = RIG_HANDSHAKE_NONE;

    return true;
}

bool HamlibBackend::configureNetwork()
{
    const Settings &s = Settings::instance();
    const QString host = s.hamlibNetworkHost();
    const int     port = s.hamlibNetworkPort();

    if (host.isEmpty()) {
        emit error(tr("No network host configured for Hamlib."));
        return false;
    }

    m_rig->state.rigport.type.rig = RIG_PORT_NETWORK;
    const QByteArray addr = QStringLiteral("%1:%2").arg(host).arg(port).toLocal8Bit();
    qstrncpy(m_rig->state.rigport.pathname, addr.constData(), FILPATHLEN - 1);
    return true;
}

void HamlibBackend::readFreq()
{
    freq_t hz = 0;
    if (rig_get_freq(m_rig, RIG_VFO_CURR, &hz) != RIG_OK)
        return;

    if (hz != m_lastFreqHz) {
        m_lastFreqHz = hz;
        emit freqChanged(hz / 1'000'000.0);
    }
}

void HamlibBackend::readMode()
{
    rmode_t  mode  = RIG_MODE_NONE;
    pbwidth_t width = 0;
    if (rig_get_mode(m_rig, RIG_VFO_CURR, &mode, &width) != RIG_OK)
        return;

    if (mode != m_lastMode) {
        m_lastMode = mode;
        QString submode;
        const QString adif = rigModeToAdif(mode, submode);
        emit modeChanged(adif, submode);
    }
}

void HamlibBackend::readPtt()
{
    ptt_t ptt = RIG_PTT_OFF;
    if (rig_get_ptt(m_rig, RIG_VFO_CURR, &ptt) != RIG_OK)
        return;
    const bool tx = (ptt != RIG_PTT_OFF);
    if (tx != m_lastPtt) {
        m_lastPtt = tx;
        emit transmitChanged(tx);
    }
}

// static
QString HamlibBackend::rigModeToAdif(rmode_t mode, QString &submode)
{
    submode.clear();
    switch (mode) {
    case RIG_MODE_USB:    submode = QStringLiteral("USB"); return QStringLiteral("SSB");
    case RIG_MODE_LSB:    submode = QStringLiteral("LSB"); return QStringLiteral("SSB");
    case RIG_MODE_CW:     return QStringLiteral("CW");
    case RIG_MODE_CWR:    return QStringLiteral("CW");
    case RIG_MODE_AM:     return QStringLiteral("AM");
    case RIG_MODE_FM:     return QStringLiteral("FM");
    case RIG_MODE_WFM:    return QStringLiteral("FM");
    case RIG_MODE_RTTY:   return QStringLiteral("RTTY");
    case RIG_MODE_RTTYR:  return QStringLiteral("RTTY");
    case RIG_MODE_PKTUSB: submode = QStringLiteral("USB"); return QStringLiteral("SSB");
    case RIG_MODE_PKTLSB: submode = QStringLiteral("LSB"); return QStringLiteral("SSB");
    case RIG_MODE_PKTFM:  return QStringLiteral("FM");
    case RIG_MODE_PKTAM:  return QStringLiteral("AM");
    case RIG_MODE_C4FM:   return QStringLiteral("FM");
    default:              return QStringLiteral("SSB");
    }
}

// static
rmode_t HamlibBackend::adifToRigMode(const QString &adifMode, const QString &submode)
{
    if (adifMode == QLatin1String("SSB")) {
        return (submode == QLatin1String("LSB")) ? RIG_MODE_LSB : RIG_MODE_USB;
    }
    if (adifMode == QLatin1String("CW"))   return RIG_MODE_CW;
    if (adifMode == QLatin1String("AM"))   return RIG_MODE_AM;
    if (adifMode == QLatin1String("FM"))   return RIG_MODE_FM;
    if (adifMode == QLatin1String("RTTY")) return RIG_MODE_RTTY;
    // Default: USB
    return RIG_MODE_USB;
}

#endif // HAVE_HAMLIB
