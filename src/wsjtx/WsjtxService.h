#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>

#include "core/logbook/Qso.h"

class QUdpSocket;

/// Listener for WSJT-X UDP broadcast protocol (schema version 2).
///
/// WSJT-X (and compatible programs) broadcast binary datagrams on a
/// configurable UDP port (default 2237).  By binding to the multicast
/// group address and using ShareAddress | ReuseAddressHint, multiple
/// applications (e.g. this logger and GridTracker2) can receive the
/// same stream simultaneously without stealing packets from each other.
///
/// Handled message types:
///   0 — Heartbeat     → heartbeat()
///   1 — Status        → statusReceived()
///   3 — Clear         → cleared()
///   5 — QSOLogged     → qsoLogged()
///  12 — LoggedADIF    → qsoLogged() (via ADIF parser)
///
/// All other types are silently ignored.
class WsjtxService : public QObject
{
    Q_OBJECT

public:
    static constexpr quint32 MAGIC          = 0xadbccbda;
    static constexpr quint32 SCHEMA_VERSION = 2;

    /// Snapshot of WSJT-X Status message (type 1).
    struct Status {
        QString  clientId;
        quint64  dialFreqHz  = 0;      // dial (not audio offset) in Hz
        QString  mode;
        QString  submode;
        QString  dxCall;               // currently selected DX callsign
        QString  dxGrid;
        QString  deCall;               // our callsign per WSJT-X config
        QString  deGrid;
        bool     transmitting = false;
        bool     decoding     = false;
        qint32   rxDF         = 0;     // RX audio DF (Hz)
        qint32   txDF         = 0;     // TX audio DF (Hz)
        QString  txMessage;
        QString  configName;
    };

    explicit WsjtxService(QObject *parent = nullptr);
    ~WsjtxService() override;

    /// Start listening on \p port.
    /// \p udpAddress may be:
    ///   - A multicast address (224.0.0.0/4, e.g. "224.0.0.1") — the socket
    ///     joins the group so multiple apps receive the same stream.
    ///   - A unicast address (e.g. "127.0.0.1") — binds to that interface only.
    ///   - Empty — binds QHostAddress::AnyIPv4 for unicast on all interfaces.
    bool start(quint16 port = 2237,
               const QString &udpAddress = QStringLiteral("224.0.0.1"));
    void stop();
    bool isRunning() const;

signals:
    void heartbeat(const QString &clientId, const QString &version, int maxSchema);
    void statusReceived(const WsjtxService::Status &status);
    /// Emitted when WSJT-X sends a Clear message (QSO cancelled / log wiped).
    void cleared();
    void qsoLogged(const Qso &qso);
    void logMessage(const QString &msg);

private slots:
    void onDatagramReady();

private:
    void processPacket(const QByteArray &data);
    void handleHeartbeat (QDataStream &ds, const QString &clientId);
    void handleStatus    (QDataStream &ds, const QString &clientId);
    void handleClear     (QDataStream &ds, const QString &clientId);
    void handleQsoLogged (QDataStream &ds, const QString &clientId);
    void handleLoggedAdif(QDataStream &ds, const QString &clientId);

    // WSJT-X uses Qt QDataStream encoding (big-endian).
    // Strings: quint32 length (0xFFFFFFFF = null/empty) then UTF-8 bytes.
    static QString   readString  (QDataStream &ds);
    static QDateTime readDateTime(QDataStream &ds);
    static bool      readBool    (QDataStream &ds);

    QUdpSocket *m_socket         = nullptr;
    quint16     m_port           = 2237;
    QString     m_multicastGroup;
};
