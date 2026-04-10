#include "QrzService.h"

#include <QDate>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include <QDebug>

#include "app/settings/SecureSettings.h"
#include "app/settings/Settings.h"
#include "core/adif/AdifParser.h"
#include "core/adif/AdifWriter.h"

static const QUrl API_URL = QUrl(QStringLiteral("https://logbook.qrz.com/api"));

QrzService::QrzService(QObject *parent)
    : QslService(parent)
    , m_nam(new QNetworkAccessManager(this))
{}

QrzService::~QrzService()
{
    abort();
}

bool QrzService::isEnabled() const
{
    return Settings::instance().qrzEnabled();
}

// ---------------------------------------------------------------------------
// Upload
// ---------------------------------------------------------------------------

void QrzService::startUpload(const QList<Qso> &allQsos)
{
    m_pendingUpload.clear();
    for (const Qso &q : allQsos) {
        if (q.qrzQslSent != 'Y')
            m_pendingUpload.append(q);
    }

    if (m_pendingUpload.isEmpty()) {
        emit uploadFinished({}, {tr("No unsent QSOs for QRZ.")});
        return;
    }

    const QString apiKey = SecureSettings::instance().get(SecureKey::QRZ_API_KEY);
    if (apiKey.isEmpty()) {
        emit uploadFinished({}, {tr("QRZ API key not configured.")});
        m_pendingUpload.clear();
        return;
    }

    AdifWriterOptions opts;
    opts.includeAppFields = false;
    const QString adif = AdifWriter::writeString(m_pendingUpload, opts);

    const QByteArray body = buildBody(apiKey, QStringLiteral("INSERT"), {}, adif);

    QNetworkRequest req(API_URL);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NF0T Logger/0.1"));

    emit uploadProgress(0, 0);
    m_reply = m_nam->post(req, body);
    connect(m_reply, &QNetworkReply::finished, this, &QrzService::onUploadReply);
}

void QrzService::onUploadReply()
{
    m_reply->deleteLater();

    QStringList errors;
    if (m_reply->error() != QNetworkReply::NoError) {
        errors << tr("QRZ upload error: %1").arg(m_reply->errorString());
        emit uploadFinished({}, errors);
        m_reply = nullptr;
        m_pendingUpload.clear();
        return;
    }

    // QRZ API returns URL-encoded key=value pairs, e.g.:
    //   RESULT=OK&LOGID=12345&COUNT=1
    //   RESULT=FAIL&REASON=...
    const QString body = QString::fromUtf8(m_reply->readAll());
    m_reply = nullptr;

    QUrlQuery response(body);
    const QString result = response.queryItemValue(QStringLiteral("RESULT"));

    if (result.compare(QLatin1String("OK"), Qt::CaseInsensitive) != 0 &&
        result.compare(QLatin1String("REPLACE"), Qt::CaseInsensitive) != 0) {
        const QString reason = response.queryItemValue(QStringLiteral("REASON"));
        emit uploadFinished({}, {tr("QRZ error: %1").arg(reason.isEmpty() ? body : reason)});
        m_pendingUpload.clear();
        return;
    }

    const QDate today = QDate::currentDate();
    for (Qso &q : m_pendingUpload) {
        q.qrzQslSent = 'Y';
        q.qrzSentDate = today;
    }

    emit uploadFinished(m_pendingUpload, {});
    m_pendingUpload.clear();
}

// ---------------------------------------------------------------------------
// Download
// ---------------------------------------------------------------------------

void QrzService::startDownload()
{
    const QString apiKey = SecureSettings::instance().get(SecureKey::QRZ_API_KEY);
    if (apiKey.isEmpty()) {
        emit downloadFinished({}, {tr("QRZ API key not configured.")});
        return;
    }

    // MODSINCE filters by last-modified date; STATUS:CONFIRMED limits to
    // QSOs confirmed by the other station (QRZ's equivalent of QSL received).
    const QString since = QDate::currentDate().addDays(-90).toString(Qt::ISODate);
    const QString option = QStringLiteral("STATUS:CONFIRMED,MODSINCE:%1,TYPE:ADIF").arg(since);

    const QByteArray body = buildBody(apiKey, QStringLiteral("FETCH"), option);

    // Debug: log the request (key redacted)
    const QByteArray debugBody = buildBody(QStringLiteral("***REDACTED***"),
                                           QStringLiteral("FETCH"), option);
    qDebug() << "QRZ FETCH request body:" << debugBody;

    QNetworkRequest req(API_URL);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NF0T Logger/0.1"));

    m_reply = m_nam->post(req, body);
    connect(m_reply, &QNetworkReply::finished, this, &QrzService::onDownloadReply);
}

void QrzService::onDownloadReply()
{
    m_reply->deleteLater();

    QStringList errors;
    if (m_reply->error() != QNetworkReply::NoError) {
        errors << tr("QRZ download error: %1").arg(m_reply->errorString());
        emit downloadFinished({}, errors);
        m_reply = nullptr;
        return;
    }

    const QString body = QString::fromUtf8(m_reply->readAll());
    m_reply = nullptr;

    qDebug() << "QRZ FETCH raw response:" << body;

    // Response is URL-encoded; DATA key contains the ADIF
    QUrlQuery response(body);
    const QString result  = response.queryItemValue(QStringLiteral("RESULT"));
    const QString reason  = response.queryItemValue(QStringLiteral("REASON"));
    const QString count   = response.queryItemValue(QStringLiteral("COUNT"));

    qDebug() << "QRZ FETCH parsed — RESULT:" << result << "REASON:" << reason << "COUNT:" << count;

    if (result.compare(QLatin1String("OK"), Qt::CaseInsensitive) != 0) {
        // COUNT=0 with FAIL just means no matching records — not a real error
        if (count == QLatin1String("0") ||
            reason.contains(QLatin1String("No records"), Qt::CaseInsensitive)) {
            emit downloadFinished({}, {tr("QRZ: no new confirmations found.")});
        } else {
            emit downloadFinished({}, {tr("QRZ fetch error: %1\nFull response: %2")
                                           .arg(reason.isEmpty() ? result : reason, body)});
        }
        return;
    }

    const QString adif = response.queryItemValue(QStringLiteral("DATA"));
    if (adif.isEmpty()) {
        emit downloadFinished({}, {});
        return;
    }

    const AdifParser::Result parsed = AdifParser::parseString(adif);
    emit downloadFinished(parsed.qsos, parsed.warnings);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// static
QByteArray QrzService::buildBody(const QString &apiKey, const QString &action,
                                  const QString &option, const QString &adif)
{
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("KEY"),    apiKey);
    q.addQueryItem(QStringLiteral("ACTION"), action);
    if (!option.isEmpty())
        q.addQueryItem(QStringLiteral("OPTION"), option);
    if (!adif.isEmpty())
        q.addQueryItem(QStringLiteral("ADIF"), adif);
    return q.query(QUrl::FullyEncoded).toUtf8();
}

// ---------------------------------------------------------------------------
// Abort
// ---------------------------------------------------------------------------

void QrzService::abort()
{
    if (m_reply)
        m_reply->abort();
    m_pendingUpload.clear();
}
