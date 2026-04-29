// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "QrzXmlLookupProvider.h"
#include "app/settings/Settings.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QXmlStreamReader>

static constexpr const char *QRZ_API_URL = "https://xmldata.qrz.com/xml/current/";

QrzXmlLookupProvider::QrzXmlLookupProvider(QObject *parent)
    : CallsignLookupProvider(parent)
    , m_nam(new QNetworkAccessManager(this))
{}

bool QrzXmlLookupProvider::isAvailable() const
{
    const Settings &s = Settings::instance();
    return s.callsignLookupEnabled()
        && !s.qrzXmlUsername().isEmpty()
        && !s.qrzXmlPassword().isEmpty();
}

void QrzXmlLookupProvider::lookup(const QString &callsign)
{
    if (!isAvailable()) return;

    m_lookupRetried = false;

    if (m_authenticating) {
        m_pendingCallsign = callsign;   // latest wins while auth is in flight
        return;
    }

    if (m_sessionKey.isEmpty()) {
        authenticate(callsign);
        return;
    }

    // Coalesce identical in-flight requests
    if (!m_currentLookupCallsign.isEmpty()
            && m_currentLookupCallsign.compare(callsign, Qt::CaseInsensitive) == 0)
        return;

    doLookup(callsign);
}

// ---------------------------------------------------------------------------
// Private — authentication
// ---------------------------------------------------------------------------

void QrzXmlLookupProvider::authenticate(const QString &pendingCallsign)
{
    m_pendingCallsign = pendingCallsign;
    m_authenticating  = true;

    if (m_authReply) {
        m_authReply->abort();
        m_authReply = nullptr;
    }

    QUrl url{QLatin1String(QRZ_API_URL)};
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("username"), Settings::instance().qrzXmlUsername());
    q.addQueryItem(QStringLiteral("password"), Settings::instance().qrzXmlPassword());
    q.addQueryItem(QStringLiteral("agent"),    QStringLiteral("NF0T-Logger"));
    url.setQuery(q);

    m_authReply = m_nam->get(QNetworkRequest(url));
    connect(m_authReply, &QNetworkReply::finished, this, [this, reply = m_authReply]() {
        if (m_authReply != reply) { reply->deleteLater(); return; }
        m_authReply      = nullptr;
        m_authenticating = false;

        if (reply->error() != QNetworkReply::NoError) {
            const QString call = m_pendingCallsign;
            m_pendingCallsign.clear();
            emit lookupFailed(call, reply->errorString());
            reply->deleteLater();
            return;
        }

        const ParsedResponse pr = parseXml(reply->readAll());
        reply->deleteLater();

        if (!pr.sessionError.isEmpty()) {
            const QString call = m_pendingCallsign;
            m_pendingCallsign.clear();
            emit lookupFailed(call, pr.sessionError);
            return;
        }

        m_sessionKey = pr.sessionKey;
        const QString call = m_pendingCallsign;
        m_pendingCallsign.clear();
        if (!call.isEmpty())
            doLookup(call);
    });
}

// ---------------------------------------------------------------------------
// Private — callsign lookup
// ---------------------------------------------------------------------------

void QrzXmlLookupProvider::doLookup(const QString &callsign)
{
    if (m_lookupReply) {
        m_lookupReply->abort();
        m_lookupReply = nullptr;
    }
    m_currentLookupCallsign = callsign;

    QUrl url{QLatin1String(QRZ_API_URL)};
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("s"),        m_sessionKey);
    q.addQueryItem(QStringLiteral("callsign"), callsign);
    url.setQuery(q);

    m_lookupReply = m_nam->get(QNetworkRequest(url));
    connect(m_lookupReply, &QNetworkReply::finished,
            this, [this, reply = m_lookupReply, callsign]() {
        if (m_lookupReply != reply) { reply->deleteLater(); return; }
        m_lookupReply           = nullptr;
        m_currentLookupCallsign.clear();

        if (reply->error() == QNetworkReply::OperationCanceledError) {
            reply->deleteLater();
            return;     // intentionally aborted; a new lookup is already starting
        }

        if (reply->error() != QNetworkReply::NoError) {
            emit lookupFailed(callsign, reply->errorString());
            reply->deleteLater();
            return;
        }

        const ParsedResponse pr = parseXml(reply->readAll());
        reply->deleteLater();

        // Session expired — re-authenticate once, then retry
        if (pr.sessionError.contains(QLatin1String("Session Timeout"), Qt::CaseInsensitive)
                && !m_lookupRetried) {
            m_lookupRetried = true;
            m_sessionKey.clear();
            authenticate(callsign);
            return;
        }

        if (!pr.sessionError.isEmpty()) {
            emit lookupFailed(callsign, pr.sessionError);
            return;
        }

        // No <Callsign> element → callsign not in database
        if (!pr.hasCallsign) {
            emit resultReady(callsign, {});
            return;
        }

        emit resultReady(callsign, pr.result);
    });
}

// ---------------------------------------------------------------------------
// Private — XML parsing
// ---------------------------------------------------------------------------

QrzXmlLookupProvider::ParsedResponse QrzXmlLookupProvider::parseXml(const QByteArray &data)
{
    ParsedResponse pr;
    QXmlStreamReader xml(data);

    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;

        if (xml.name() == QLatin1String("Session")) {
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isEndElement() && xml.name() == QLatin1String("Session")) break;
                if (!xml.isStartElement()) continue;
                const auto tag = xml.name();
                if      (tag == QLatin1String("Key"))   pr.sessionKey   = xml.readElementText();
                else if (tag == QLatin1String("Error"))  pr.sessionError = xml.readElementText();
            }
        } else if (xml.name() == QLatin1String("Callsign")) {
            pr.hasCallsign = true;
            CallsignLookupResult &r = pr.result;
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isEndElement() && xml.name() == QLatin1String("Callsign")) break;
                if (!xml.isStartElement()) continue;
                const auto    tag = xml.name();
                const QString val = xml.readElementText();
                if      (tag == QLatin1String("call"))    r.callsign     = val;
                else if (tag == QLatin1String("fname"))   r.firstName    = val;
                else if (tag == QLatin1String("name"))    r.lastName     = val;
                else if (tag == QLatin1String("addr2"))   r.qth          = val;
                else if (tag == QLatin1String("state"))   r.state        = val;
                else if (tag == QLatin1String("county"))  r.county       = val;
                else if (tag == QLatin1String("country")) r.country      = val;
                else if (tag == QLatin1String("grid"))    r.gridsquare   = val;
                else if (tag == QLatin1String("dxcc"))    r.dxcc         = val.toInt();
                else if (tag == QLatin1String("cqzone"))  r.cqZone       = val.toInt();
                else if (tag == QLatin1String("ituzone")) r.ituZone      = val.toInt();
                else if (tag == QLatin1String("class"))   r.licenseClass = val;
                else if (tag == QLatin1String("email"))   r.email        = val;
                else if (tag == QLatin1String("url"))     r.url          = val;
                else if (tag == QLatin1String("image"))   r.imageUrl     = val;
                else if (tag == QLatin1String("lat")) {
                    bool ok; const double v = val.toDouble(&ok);
                    if (ok) r.lat = v;
                } else if (tag == QLatin1String("lon")) {
                    bool ok; const double v = val.toDouble(&ok);
                    if (ok) r.lon = v;
                }
            }
            r.name = QStringList{r.firstName, r.lastName}
                         .join(QLatin1Char(' ')).trimmed();
        }
    }
    return pr;
}
