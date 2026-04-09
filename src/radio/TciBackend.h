#pragma once

#include <QAbstractSocket>
#include <QObject>
#include <QString>

class QTimer;
class QWebSocket;

/// ExpertSDR TCI radio backend.
///
/// Connects to a TCI server (ExpertSDR2/3 or compatible) via WebSocket.
/// TCI is a text-based protocol — each command is a semicolon-terminated
/// ASCII string, e.g. "vfo:0,0,14074000;" or "modulation:0,USB;".
///
/// The backend tracks TRX 0 / VFO A only. It emits freqChanged() and
/// modeChanged() signals consumed by QsoEntryPanel, mirroring the same
/// interface as HamlibBackend so MainWindow can treat them uniformly.
class TciBackend : public QObject
{
    Q_OBJECT

public:
    explicit TciBackend(QObject *parent = nullptr);
    ~TciBackend() override;

    bool connectTci();
    void disconnectTci();
    bool isConnected() const;

public slots:
    /// Push a new frequency (MHz) to TRX 0 / VFO A.
    void setFreq(double freqMhz);

    /// Push a new mode to TRX 0 (ADIF mode + optional submode string).
    void setMode(const QString &adifMode, const QString &submode = {});

signals:
    void freqChanged(double freqMhz);
    void modeChanged(const QString &mode, const QString &submode);
    void connected();
    void disconnected();
    void error(const QString &message);

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

    QWebSocket *m_socket        = nullptr;
    QTimer     *m_reconnectTimer = nullptr;

    bool    m_connected          = false;
    bool    m_intentionalClose   = false;
    bool    m_serverReady        = false;
    double  m_lastFreqHz         = 0.0;
    QString m_lastTciMode;
};
