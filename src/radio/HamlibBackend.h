// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <atomic>

#include <QThread>

#include "RadioBackend.h"

class HamlibWorker;

/// Hamlib CAT radio backend — UI-thread facade.
///
/// Every slot here is a plain virtual call: MainWindow parents this object
/// normally and invokes connectRadio()/disconnectRadio()/setFreq()/setMode()
/// directly, exactly like TciBackend. All blocking Hamlib I/O (rig_open,
/// rig_close, the 500ms poll, setFreq/setMode) happens on HamlibWorker,
/// which lives on m_workerThread; each slot here hands off to it via a
/// queued QMetaObject::invokeMethod() call, and the worker's signals are
/// relayed back out through this object's own RadioBackend signals.
///
/// When HAVE_HAMLIB is not defined, HamlibWorker still compiles;
/// connectRadio() always ends in an error() signal.
class HamlibBackend : public RadioBackend
{
    Q_OBJECT

public:
    explicit HamlibBackend(QObject *parent = nullptr);
    ~HamlibBackend() override;

    QString displayName() const override { return QStringLiteral("Hamlib"); }
    bool    isConnected()  const override;

public slots:
    void connectRadio()       override;
    void disconnectRadio()    override;
    void setFreq(double freqMhz) override;
    void setMode(const QString &adifMode, const QString &submode = {}) override;

private:
    QThread            m_workerThread;
    HamlibWorker      *m_worker = nullptr;
    std::atomic<bool>  m_connected{false};
};
