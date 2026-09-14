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
    // The wait() below is unconditional, not merely defensive: m_workerThread
    // is an embedded QThread member, and Qt does not support destroying a
    // QThread object while its thread is still running — doing so abandons
    // the running OS thread while its QThreadPrivate is torn out from under
    // it. So this object cannot finish destructing until the worker thread
    // has actually returned from its event loop, however long that takes.
    //
    // Known accepted limitation (see issue #22 / PR #21 review): if the
    // worker is blocked inside a Hamlib call (e.g. rig_open() hung on a dead
    // network host) when this runs, that requirement means quit()+wait()
    // below blocks the UI thread until the call returns or times out. This
    // reintroduces, at shutdown only, a bounded version of the freeze #18
    // removed from normal polling. There's no safe way to bound it further:
    // Hamlib's blocking C API has no cancellation hook.
    //
    // We still disconnect synchronously first (rather than leaving cleanup
    // to ~HamlibWorker(), which runs later via deleteLater() once the thread
    // finishes) so the rig is closed gracefully and disconnected() fires
    // deterministically before this call returns. Guard it with isRunning():
    // Qt::BlockingQueuedConnection has no timeout, and if m_workerThread never
    // started (or already stopped) there is no event loop left to deliver
    // it, which would deadlock the UI thread forever instead of just for a
    // bounded time.
    if (m_workerThread.isRunning()) {
        QMetaObject::invokeMethod(m_worker, &HamlibWorker::doDisconnect,
                                   Qt::BlockingQueuedConnection);
    }
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
