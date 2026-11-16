// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <functional>

#include <QAbstractSocket>
#include <QElapsedTimer>
#include <QMap>
#include <QStringList>

#include "RadioBackend.h"

class QTimer;
class QWebSocket;

/// TCI 2.1 client backend for ExpertSDR3, AetherSDR and other TCI servers.
///
/// TCI is a text protocol over WebSocket: each command is `name:arg1,arg2;`,
/// several commands may share one frame, and command names are
/// case-insensitive. The logger implements the subset of TCI 2.1 a logger
/// needs: initialization, VFO/modulation/TRX/split state per transceiver,
/// TX frequency, and panadapter spot clicks. Audio/IQ streams, CW and DSP
/// control are deliberately not implemented; their commands are ignored, which
/// is what the spec requires of a client that does not use them.
///
/// Transceiver focus follows ECODER_SWITCH_RX (panel 0) and AetherSDR's
/// `active_slice` extension; both are optional, and TRX 0 is used otherwise.
/// Split is reported per transceiver, so it applies only while focus is on
/// the split one. AetherSDR reports split on TRX 0 whenever its TX slice is
/// another slice, which it cannot distinguish from working that slice alone;
/// focus is the only signal of which one the operator is working.
class TciBackend : public RadioBackend
{
    Q_OBJECT
    friend class TestTciBackend;

public:
    explicit TciBackend(QObject *parent = nullptr);
    ~TciBackend() override;

    QString displayName() const override { return QStringLiteral("TCI"); }
    bool    isConnected()  const override;

    QString     deviceName()           const { return m_deviceName; }
    QString     protocolVersion()      const { return m_protocolVersion; }
    QStringList supportedModulations() const { return m_supportedModulations; }
    int         trxCount()             const { return m_trxCount; }
    bool        isReceiveOnly()        const { return m_receiveOnly; }
    int         activeTrx()            const { return m_activeTrx; }
    bool        isSplit()              const;

public slots:
    void connectRadio()       override;
    void disconnectRadio()    override;
    void setFreq(double freqMhz) override;
    void setMode(const QString &adifMode, const QString &submode = {}) override;

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void onError(QAbstractSocket::SocketError socketError);
    void onReconnectTimer();

private:
    struct TrxState {
        double  vfoAHz   = 0.0;    // channel A: the receive frequency
        double  vfoBHz   = 0.0;    // channel B: the TX frequency while split is on
        QString tciMode;           // upper case, as normalised on receipt
        bool    split    = false;
        bool    tx       = false;
        bool    enabled  = true;   // RX_ENABLE; TRX 0 is always enabled
    };

    void resetSessionState();
    void parseMessage(const QString &msg);
    void send(const QString &cmd);
    void switchActiveTrx(int trx);
    void updateTransmitting();
    void emitSpotClicked(const QString &callsign, double freqHz);
    bool trxInRange(int trx) const;
    QString negotiateModulation(const QString &tciMode) const;

    static QStringList splitArgs(const QString &params);
    static bool parseBool(const QString &str);
    static QString tciModeToAdif(const QString &tciMode, QString &submode);
    static QString adifToTciMode(const QString &adifMode, const QString &submode);

    QWebSocket *m_socket         = nullptr;
    QTimer     *m_reconnectTimer = nullptr;
    std::function<void(const QString &)> m_sender;   // replaced by tests

    bool m_connected        = false;
    bool m_intentionalClose = false;
    bool m_serverReady      = false;
    bool m_transmitting     = false;

    // Initialization commands
    QString     m_deviceName;
    QString     m_protocolVersion;
    QStringList m_supportedModulations;   // upper case
    int         m_trxCount      = 1;
    int         m_channelsCount = 1;
    bool        m_receiveOnly   = false;

    int                   m_activeTrx = 0;
    QMap<int, TrxState>   m_trx;

    // AetherSDR sends CLICKED_ON_SPOT and RX_CLICKED_ON_SPOT for one click.
    QString       m_lastSpotCall;
    double        m_lastSpotHz = 0.0;
    QElapsedTimer m_lastSpotTimer;
};
