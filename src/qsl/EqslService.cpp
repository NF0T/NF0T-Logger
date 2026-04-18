// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "EqslService.h"

#include <QDate>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrlQuery>

#include "app/settings/SecureSettings.h"
#include "app/settings/Settings.h"
#include "core/adif/AdifParser.h"
#include "core/adif/AdifWriter.h"

static const QUrl UPLOAD_URL   = QUrl(QStringLiteral("https://www.eqsl.cc/qslcard/ImportADIF.cfm"));
static const QUrl DOWNLOAD_URL = QUrl(QStringLiteral("https://www.eqsl.cc/qslcard/DownloadInBox.cfm"));

EqslService::EqslService(QObject *parent)
    : QslService(parent)
    , m_nam(new QNetworkAccessManager(this))
{}

EqslService::~EqslService()
{
    abort();
}

bool EqslService::isEnabled() const
{
    return Settings::instance().eqslEnabled();
}

// ---------------------------------------------------------------------------
// Upload
// ---------------------------------------------------------------------------

void EqslService::startUpload(const QList<Qso> &allQsos)
{
    m_pendingUpload.clear();
    for (const Qso &q : allQsos) {
        if (q.eqslQslSent != 'Y')
            m_pendingUpload.append(q);
    }

    if (m_pendingUpload.isEmpty()) {
        emit logMessage(tr("No unsent QSOs for eQSL."));
        emit uploadFinished({}, {tr("No unsent QSOs for eQSL.")});
        return;
    }

    const Settings &s    = Settings::instance();
    const QString   user = s.eqslUsername();
    const QString   pass = SecureSettings::instance().get(SecureKey::EQSL_PASSWORD);

    if (user.isEmpty() || pass.isEmpty()) {
        const QString err = tr("eQSL credentials not configured.");
        emit logMessage(err);
        emit uploadFinished({}, {err});
        m_pendingUpload.clear();
        return;
    }

    emit logMessage(tr("Uploading %1 QSO(s) to eQSL as %2...").arg(m_pendingUpload.size()).arg(user));

    AdifWriterOptions opts;
    opts.includeAppFields = false;
    const QString adif = AdifWriter::writeString(m_pendingUpload, opts);

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("EQSL_USER"), user);
    body.addQueryItem(QStringLiteral("EQSL_PSWD"), pass);
    body.addQueryItem(QStringLiteral("ADIFData"),  adif);

    QNetworkRequest req(UPLOAD_URL);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NF0T Logger/0.1"));

    emit uploadProgress(0, 0);
    m_reply = m_nam->post(req, body.query(QUrl::FullyEncoded).toUtf8());
    connect(m_reply, &QNetworkReply::finished, this, &EqslService::onUploadReply);
}

void EqslService::onUploadReply()
{
    m_reply->deleteLater();

    if (m_reply->error() != QNetworkReply::NoError) {
        const QString err = tr("eQSL upload error: %1").arg(m_reply->errorString());
        emit logMessage(err);
        emit uploadFinished({}, {err});
        m_reply = nullptr;
        m_pendingUpload.clear();
        return;
    }

    const QString body = QString::fromUtf8(m_reply->readAll()).trimmed();
    m_reply = nullptr;

    emit logMessage(tr("eQSL response: %1").arg(body.left(200)));

    // eQSL returns "Result: 0 records added" or an error description
    if (body.contains(QLatin1String("Error"), Qt::CaseInsensitive) &&
        !body.contains(QLatin1String("0 Duplicate"), Qt::CaseInsensitive)) {
        emit uploadFinished({}, {tr("eQSL error: %1").arg(body)});
        m_pendingUpload.clear();
        return;
    }

    // Mark sent
    const QDate today = QDate::currentDate();
    for (Qso &q : m_pendingUpload) {
        q.eqslQslSent = 'Y';
        q.eqslSentDate = today;
    }

    emit logMessage(tr("Successfully uploaded %1 QSO(s) to eQSL.").arg(m_pendingUpload.size()));
    emit uploadFinished(m_pendingUpload, {});
    m_pendingUpload.clear();
}

// ---------------------------------------------------------------------------
// Download
// ---------------------------------------------------------------------------

void EqslService::startDownload(const QDate &from, const QDate &to)
{
    const Settings &s    = Settings::instance();
    const QString   user = s.eqslUsername();
    const QString   pass = SecureSettings::instance().get(SecureKey::EQSL_PASSWORD);

    if (user.isEmpty() || pass.isEmpty()) {
        const QString err = tr("eQSL credentials not configured.");
        emit logMessage(err);
        emit downloadFinished({}, {err});
        return;
    }

    // eQSL API date format: MM/DD/YYYY; QUrlQuery will percent-encode slashes as %2F
    const QString fromStr = from.toString(QStringLiteral("MM/dd/yyyy"));
    const QString toStr   = to.toString(QStringLiteral("MM/dd/yyyy"));
    emit logMessage(tr("Requesting eQSL inbox for %1 (%2 to %3)...").arg(user, fromStr, toStr));

    QUrl url = DOWNLOAD_URL;
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("UserName"),      user);
    q.addQueryItem(QStringLiteral("Password"),      pass);
    q.addQueryItem(QStringLiteral("LimitDateLo"),   fromStr);
    q.addQueryItem(QStringLiteral("LimitDateHi"),   toStr);
    q.addQueryItem(QStringLiteral("ConfirmedOnly"), QStringLiteral("1"));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NF0T Logger/0.1"));

    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::finished, this, &EqslService::onDownloadReply);
}

// Step 1: eQSL returns an HTML page. On success it contains "built" and a link to the
// generated .adi file. Parse out that link and fetch it in step 2.
void EqslService::onDownloadReply()
{
    m_reply->deleteLater();

    if (m_reply->error() != QNetworkReply::NoError) {
        const QString err = tr("eQSL error: %1").arg(m_reply->errorString());
        emit logMessage(err);
        emit downloadFinished({}, {err});
        m_reply = nullptr;
        return;
    }

    const QString body    = QString::fromUtf8(m_reply->readAll());
    const QUrl    baseUrl = m_reply->url();
    m_reply = nullptr;

    emit logMessage(tr("Received index page (%1 byte(s)) from eQSL.").arg(body.size()));

    if (!body.contains(QLatin1String("built"), Qt::CaseInsensitive)) {
        // Something went wrong — log non-tag lines so the error text is readable
        emit logMessage(tr("eQSL did not build an ADIF file. Response:"));
        const QStringList lines = body.left(3000).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QString t = line.trimmed();
            if (!t.isEmpty() && !t.startsWith(QLatin1Char('<')))
                emit logMessage(QStringLiteral("  ") + t);
        }
        emit downloadFinished({}, {tr("eQSL did not build an ADIF file.")});
        return;
    }

    // Extract the href to the .adi (or .txt) file eQSL generated
    static const QRegularExpression linkRe(
        QStringLiteral("href=\"([^\"]+\\.(?:adi|txt))\""),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = linkRe.match(body);
    if (!match.hasMatch()) {
        emit logMessage(tr("eQSL: ADIF link not found in response page."));
        emit downloadFinished({}, {tr("eQSL: ADIF link not found in response.")});
        return;
    }

    const QUrl adifUrl = baseUrl.resolved(QUrl(match.captured(1)));
    emit logMessage(tr("Fetching ADIF file from eQSL..."));

    QNetworkRequest req(adifUrl);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NF0T Logger/0.1"));

    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::finished, this, &EqslService::onAdifFileReply);
}

// Step 2: Parse the actual ADIF file returned by eQSL.
void EqslService::onAdifFileReply()
{
    m_reply->deleteLater();

    if (m_reply->error() != QNetworkReply::NoError) {
        const QString err = tr("eQSL ADIF fetch error: %1").arg(m_reply->errorString());
        emit logMessage(err);
        emit downloadFinished({}, {err});
        m_reply = nullptr;
        return;
    }

    const QString adif = QString::fromUtf8(m_reply->readAll());
    m_reply = nullptr;

    emit logMessage(tr("Received ADIF file (%1 byte(s)) from eQSL.").arg(adif.size()));

    const AdifParser::Result parsed = AdifParser::parseString(adif);

    if (!parsed.warnings.isEmpty()) {
        emit logMessage(tr("Parser warnings:"));
        for (const QString &w : parsed.warnings)
            emit logMessage(tr("  %1").arg(w));
    }

    // We requested ConfirmedOnly=1, so every returned record is confirmed.
    // eQSL's ADIF does not carry EQSL_QSL_RCVD:1:Y, so force it explicitly.
    const QDate today = QDate::currentDate();
    QList<Qso> confirmed = parsed.qsos;
    for (Qso &q : confirmed) {
        q.eqslQslRcvd = 'Y';
        if (!q.eqslRcvdDate.isValid())
            q.eqslRcvdDate = today;
    }

    emit logMessage(tr("Parsed %1 confirmation(s) from eQSL:").arg(confirmed.size()));
    for (const Qso &q : confirmed) {
        emit logMessage(tr("  %1  %2  %3  %4")
                            .arg(q.callsign,
                                 q.datetimeOn.toUTC().toString(QStringLiteral("yyyy-MM-dd")),
                                 q.band, q.mode));
    }

    emit downloadFinished(confirmed, parsed.warnings);
}

// ---------------------------------------------------------------------------
// Abort
// ---------------------------------------------------------------------------

void EqslService::abort()
{
    if (m_reply)
        m_reply->abort();
    m_pendingUpload.clear();
}
