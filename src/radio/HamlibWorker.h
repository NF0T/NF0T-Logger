// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <QObject>
#include <QString>

#ifdef HAVE_HAMLIB
#include <hamlib/rig.h>
#endif

class QTimer;

/// Worker object for HamlibBackend.
///
/// Lives on HamlibBackend's dedicated QThread and owns the RIG* handle, the
/// poll QTimer, and every blocking Hamlib call (rig_open/rig_close, polling,
/// setFreq/setMode). HamlibBackend reaches it only through queued
/// QMetaObject::invokeMethod() calls dispatched onto that thread; it reports
/// back only through signals, which Qt queues automatically across the
/// thread boundary. It is never touched directly from the UI thread.
class HamlibWorker : public QObject
{
    Q_OBJECT

public:
    explicit HamlibWorker(QObject *parent = nullptr);
    ~HamlibWorker() override;

public slots:
    void doConnect();
    void doDisconnect();
    void doSetFreq(double freqMhz);
    void doSetMode(const QString &adifMode, const QString &submode);

signals:
    void freqChanged(double freqMhz);
    void modeChanged(const QString &mode, const QString &submode);
    void transmitChanged(bool transmitting);
    void connected();
    void disconnected();
    void error(const QString &message);

private slots:
    void poll();

private:
#ifdef HAVE_HAMLIB
    bool configureSerial();
    bool configureNetwork();
    bool readFreq();
    bool readMode();
    bool readPtt();

    static QString rigModeToAdif(rmode_t mode, QString &submode);
    static rmode_t adifToRigMode(const QString &adifMode, const QString &submode);

    RIG    *m_rig        = nullptr;
    rmode_t m_lastMode   = RIG_MODE_NONE;
    bool    m_lastPtt    = false;
    int     m_consecutiveFailures = 0;
#endif

    QTimer *m_pollTimer  = nullptr;
    bool    m_connected  = false;
    double  m_lastFreqHz = 0.0;
};
