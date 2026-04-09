#pragma once

#include <QObject>
#include <QString>

#ifdef HAVE_HAMLIB
#include <hamlib/rig.h>
#endif

class QTimer;

/// Hamlib CAT radio backend.
///
/// Polls the connected rig every 500 ms for frequency and mode changes,
/// emitting freqChanged() / modeChanged() signals consumed by QsoEntryPanel.
///
/// When HAVE_HAMLIB is not defined the class still compiles; connectRig()
/// always returns false and emits an error() signal.
class HamlibBackend : public QObject
{
    Q_OBJECT

public:
    explicit HamlibBackend(QObject *parent = nullptr);
    ~HamlibBackend() override;

    /// Open the rig using current Settings values.
    /// Returns true on success; on failure emits error() with details.
    bool connectRig();

    /// Gracefully close the rig connection.
    void disconnectRig();

    bool isConnected() const;

public slots:
    /// Push a new frequency (MHz) to the rig's current VFO.
    void setFreq(double freqMhz);

    /// Push a new mode to the rig (ADIF mode string, e.g. "SSB", "CW").
    void setMode(const QString &adifMode, const QString &submode = {});

signals:
    void freqChanged(double freqMhz);
    void modeChanged(const QString &mode, const QString &submode);
    void connected();
    void disconnected();
    void error(const QString &message);

private slots:
    void poll();

private:
#ifdef HAVE_HAMLIB
    bool configureSerial();
    bool configureNetwork();
    void readFreq();
    void readMode();

    static QString rigModeToAdif(rmode_t mode, QString &submode);
    static rmode_t adifToRigMode(const QString &adifMode, const QString &submode);

    RIG    *m_rig        = nullptr;
    rmode_t m_lastMode   = RIG_MODE_NONE;
#endif

    QTimer *m_pollTimer  = nullptr;
    bool    m_connected  = false;
    double  m_lastFreqHz = 0.0;
};
