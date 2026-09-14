// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <atomic>

#include <QThread>

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
///
/// The whole object lives on its own QThread (m_thread) so a slow or hung
/// Hamlib call never stalls the UI. It therefore cannot take a QObject
/// parent — moveToThread() refuses to move a parented object — so it has no
/// parent constructor parameter and MainWindow owns/deletes it explicitly.
class HamlibBackend : public RadioBackend
{
    Q_OBJECT

public:
    explicit HamlibBackend();
    ~HamlibBackend() override;

    QString displayName() const override { return QStringLiteral("Hamlib"); }
    bool    isConnected()  const override;

public slots:
    void connectRadio()       override;
    void disconnectRadio()    override;
    void setFreq(double freqMhz) override;
    void setMode(const QString &adifMode, const QString &submode = {}) override;

private slots:
    void poll();

private:
#ifdef HAVE_HAMLIB
    bool configureSerial();
    bool configureNetwork();
    bool readFreq();
    bool readMode();

    static QString rigModeToAdif(rmode_t mode, QString &submode);
    static rmode_t adifToRigMode(const QString &adifMode, const QString &submode);

    bool readPtt();

    RIG    *m_rig        = nullptr;
    rmode_t m_lastMode   = RIG_MODE_NONE;
    bool    m_lastPtt    = false;
    int     m_consecutiveFailures = 0;
#endif

    QTimer *m_pollTimer  = nullptr;
    QThread m_thread;
    std::atomic<bool> m_connected{false};
    double  m_lastFreqHz = 0.0;
};
