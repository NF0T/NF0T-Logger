// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include "CallsignLookupProvider.h"
#include "CallsignLookupResult.h"

class QNetworkAccessManager;
class QNetworkReply;

/// QRZ XML Data API provider.
///
/// Auth flow: GET /xml/current/?username=X&password=Y&agent=NF0T-Logger
/// returns a session key valid for ~24 h. Subsequent lookups use that key.
/// On Session Timeout the provider re-authenticates once and retries.
class QrzXmlLookupProvider : public CallsignLookupProvider
{
    Q_OBJECT

public:
    explicit QrzXmlLookupProvider(QObject *parent = nullptr);

    QString displayName() const override { return QStringLiteral("QRZ XML"); }
    bool    isAvailable() const override;
    void    lookup(const QString &callsign) override;

private:
    void authenticate(const QString &pendingCallsign);
    void doLookup(const QString &callsign);

    struct ParsedResponse {
        QString              sessionKey;
        QString              sessionError;
        CallsignLookupResult result;
        bool                 hasCallsign = false;
    };
    static ParsedResponse parseXml(const QByteArray &data);

    QNetworkAccessManager *m_nam;

    QString m_sessionKey;
    QString m_pendingCallsign;
    QString m_currentLookupCallsign;

    bool m_authenticating = false;
    bool m_lookupRetried  = false;

    QNetworkReply *m_authReply   = nullptr;
    QNetworkReply *m_lookupReply = nullptr;
};
