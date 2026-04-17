#pragma once

#include <QAbstractSocket>

#include "RadioBackend.h"

class QTimer;
class QWebSocket;

/// ExpertSDR TCI radio backend.
///
/// Connects to a TCI server (ExpertSDR2/3 or compatible) via WebSocket.
/// TCI is a text-based protocol — each command is a semicolon-terminated
/// ASCII string, e.g. "vfo:0,0,14074000;" or "modulation:0,USB;".
///
/// The backend tracks TRX 0 / VFO A only.
class TciBackend : public RadioBackend
{
    Q_OBJECT

public:
    explicit TciBackend(QObject *parent = nullptr);
    ~TciBackend() override;

    QString displayName() const override { return QStringLiteral("TCI"); }
    bool    isConnected()  const override;
    bool    connectRadio()       override;
    void    disconnectRadio()    override;

public slots:
    void setFreq(double freqMhz) override;
    void setMode(const QString &adifMode, const QString &submode = {}) override;

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void onError(QAbstractSocket::SocketError socketError);
    void onReconnectTimer();

private:
    void parseMessage(const QString &msg);
    void send(const QString &cmd);

    static QString tciModeToAdif(const QString &tciMode, QString &submode);
    static QString adifToTciMode(const QString &adifMode, const QString &submode);

    QWebSocket *m_socket         = nullptr;
    QTimer     *m_reconnectTimer = nullptr;

    bool    m_connected        = false;
    bool    m_intentionalClose = false;
    bool    m_serverReady      = false;
    bool    m_transmitting     = false;
    double  m_lastFreqHz       = 0.0;
    QString m_lastTciMode;
};
