#pragma once

#include "RadioBackend.h"

#ifdef HAVE_HAMLIB
#include <hamlib/rig.h>
#endif

class QTimer;

/// Hamlib CAT radio backend.
///
/// Polls the connected rig every 500 ms for frequency and mode changes,
/// emitting freqChanged() / modeChanged() signals consumed by QsoEntryPanel.
///
/// When HAVE_HAMLIB is not defined the class still compiles; connectRadio()
/// always returns false and emits an error() signal.
class HamlibBackend : public RadioBackend
{
    Q_OBJECT

public:
    explicit HamlibBackend(QObject *parent = nullptr);
    ~HamlibBackend() override;

    QString displayName() const override { return QStringLiteral("Hamlib"); }
    bool    isConnected()  const override;
    bool    connectRadio()       override;
    void    disconnectRadio()    override;

public slots:
    void setFreq(double freqMhz) override;
    void setMode(const QString &adifMode, const QString &submode = {}) override;

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
