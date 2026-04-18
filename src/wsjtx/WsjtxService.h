// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include "radio/DigitalListenerService.h"

#include <QDateTime>
#include <QString>

class QUdpSocket;

/// WSJT-X UDP broadcast listener (schema version 2).
///
/// Implements DigitalListenerService for the WSJT-X binary UDP protocol.
/// Multicast and unicast are both supported; ShareAddress | ReuseAddressHint
/// lets this logger coexist with GridTracker2 and JTAlert on the same port.
///
/// Handled message types:
///   0 — Heartbeat  → logMessage()
///   1 — Status     → stationSelected() / radioStatusChanged()
///   3 — Clear      → cleared()
///   5 — QSOLogged  → qsoLogged()
///  12 — LoggedADIF → qsoLogged() (via ADIF parser)
class WsjtxService : public DigitalListenerService
{
    Q_OBJECT

public:
    static constexpr quint32 MAGIC          = 0xadbccbda;
    static constexpr quint32 SCHEMA_VERSION = 2;

    explicit WsjtxService(QObject *parent = nullptr);
    ~WsjtxService() override;

    QString displayName() const override { return QStringLiteral("WSJT-X"); }
    bool    isEnabled()   const override;
    bool    isRunning()   const override;

    /// Start listening using current Settings values (port + UDP address).
    bool start() override;
    void stop()  override;

private slots:
    void onDatagramReady();

private:
    bool startOnAddress(quint16 port, const QString &udpAddress);
    void processPacket(const QByteArray &data);
    void handleHeartbeat (QDataStream &ds, const QString &clientId);
    void handleStatus    (QDataStream &ds, const QString &clientId);
    void handleClear     (QDataStream &ds, const QString &clientId);
    void handleQsoLogged (QDataStream &ds, const QString &clientId);
    void handleLoggedAdif(QDataStream &ds, const QString &clientId);

    static QString   readString  (QDataStream &ds);
    static QDateTime readDateTime(QDataStream &ds);
    static bool      readBool    (QDataStream &ds);

    // Normalise a WSJT-X mode string to ADIF-spec (mode, submode) pair
    static void normaliseMode(const QString &raw, QString &mode, QString &submode);

    QUdpSocket *m_socket         = nullptr;
    quint16     m_port           = 2237;
    QString     m_multicastGroup;

    // Change-detection: only emit stationSelected when the DX station changes
    QString m_lastDxCall;
    QString m_lastDxGrid;
};
