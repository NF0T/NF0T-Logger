// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "HamlibBackend.h"
#include "HamlibWorker.h"

#include <QMetaObject>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

HamlibBackend::HamlibBackend(QObject *parent)
    : RadioBackend(parent)
{
    m_worker = new HamlibWorker;
    m_worker->moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &HamlibWorker::freqChanged,      this, &HamlibBackend::freqChanged);
    connect(m_worker, &HamlibWorker::modeChanged,      this, &HamlibBackend::modeChanged);
    connect(m_worker, &HamlibWorker::transmitChanged,  this, &HamlibBackend::transmitChanged);
    connect(m_worker, &HamlibWorker::error,            this, &HamlibBackend::error);
    connect(m_worker, &HamlibWorker::connected, this, [this]() {
        m_connected = true;
        emit connected();
    });
    connect(m_worker, &HamlibWorker::disconnected, this, [this]() {
        m_connected = false;
        emit disconnected();
    });

    m_workerThread.start();
}

HamlibBackend::~HamlibBackend()
{
    // Known accepted limitation (see issue #22 / PR #21 review): if
    // m_workerThread is blocked inside a Hamlib call (e.g. rig_open() hung
    // on a dead network host) when this runs, this BlockingQueuedConnection
    // call blocks the UI thread until that call returns or times out. This
    // reintroduces, at shutdown only, a bounded version of the freeze #18
    // removed from normal polling. There's no safe way to bound it further
    // here: Hamlib's blocking C API has no cancellation hook, and
    // Qt::BlockingQueuedConnection has no timeout — abandoning the wait
    // would let m_workerThread keep touching m_worker's RIG* handle after
    // deleteLater() (triggered below via QThread::finished) destroys it,
    // which is worse.
    QMetaObject::invokeMethod(m_worker, &HamlibWorker::doDisconnect,
                               Qt::BlockingQueuedConnection);
    m_workerThread.quit();
    m_workerThread.wait();
}

// ---------------------------------------------------------------------------
// RadioBackend interface — plain calls, dispatched to m_worker internally
// ---------------------------------------------------------------------------

void HamlibBackend::connectRadio()
{
    QMetaObject::invokeMethod(m_worker, &HamlibWorker::doConnect, Qt::QueuedConnection);
}

void HamlibBackend::disconnectRadio()
{
    QMetaObject::invokeMethod(m_worker, &HamlibWorker::doDisconnect, Qt::QueuedConnection);
}

void HamlibBackend::setFreq(double freqMhz)
{
    QMetaObject::invokeMethod(m_worker, &HamlibWorker::doSetFreq, Qt::QueuedConnection, freqMhz);
}

void HamlibBackend::setMode(const QString &adifMode, const QString &submode)
{
    QMetaObject::invokeMethod(m_worker, &HamlibWorker::doSetMode, Qt::QueuedConnection,
                               adifMode, submode);
}

bool HamlibBackend::isConnected() const
{
    return m_connected;
}
