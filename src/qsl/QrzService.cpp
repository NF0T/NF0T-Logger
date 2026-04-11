#include "QrzService.h"

#include <QDate>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

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
        emit logMessage(tr("No unsent QSOs for QRZ."));
        emit uploadFinished({}, {tr("No unsent QSOs for QRZ.")});
        return;
    }

    const QString apiKey = SecureSettings::instance().get(SecureKey::QRZ_API_KEY);
    if (apiKey.isEmpty()) {
        const QString err = tr("QRZ API key not configured.");
        emit logMessage(err);
        emit uploadFinished({}, {err});
        m_pendingUpload.clear();
        return;
    }

    emit logMessage(tr("Uploading %1 QSO(s) to QRZ Logbook...").arg(m_pendingUpload.size()));

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

    if (m_reply->error() != QNetworkReply::NoError) {
        const QString err = tr("QRZ upload error: %1").arg(m_reply->errorString());
        emit logMessage(err);
        emit uploadFinished({}, {err});
        m_reply = nullptr;
        m_pendingUpload.clear();
        return;
    }

    // QRZ API returns URL-encoded key=value pairs, e.g.:
    //   RESULT=OK&LOGID=12345&COUNT=1
    //   RESULT=FAIL&REASON=...
    const QString body = QString::fromUtf8(m_reply->readAll());
    m_reply = nullptr;

    emit logMessage(tr("QRZ upload response: %1").arg(body));

    QUrlQuery response(body);
    const QString result = response.queryItemValue(QStringLiteral("RESULT"));

    if (result.compare(QLatin1String("OK"), Qt::CaseInsensitive) != 0 &&
        result.compare(QLatin1String("REPLACE"), Qt::CaseInsensitive) != 0) {
        const QString reason = response.queryItemValue(QStringLiteral("REASON"));
        const QString err = tr("QRZ error: %1").arg(reason.isEmpty() ? body : reason);
        emit logMessage(err);
        emit uploadFinished({}, {err});
        m_pendingUpload.clear();
        return;
    }

    const QDate today = QDate::currentDate();
    for (Qso &q : m_pendingUpload) {
        q.qrzQslSent = 'Y';
        q.qrzSentDate = today;
    }

    emit logMessage(tr("Successfully uploaded %1 QSO(s) to QRZ Logbook.").arg(m_pendingUpload.size()));
    emit uploadFinished(m_pendingUpload, {});
    m_pendingUpload.clear();
}

// ---------------------------------------------------------------------------
// Download
// ---------------------------------------------------------------------------

void QrzService::startDownload(const QDate &from, const QDate &to)
{
    const QString apiKey = SecureSettings::instance().get(SecureKey::QRZ_API_KEY);
    if (apiKey.isEmpty()) {
        const QString err = tr("QRZ API key not configured.");
        emit logMessage(err);
        emit downloadFinished({}, {err});
        return;
    }

    const QString since = from.toString(Qt::ISODate);
    const QString toStr = to.toString(Qt::ISODate);
    emit logMessage(tr("Requesting QRZ confirmations (%1 to %2)...").arg(since, toStr));

    const QString option = QStringLiteral("STATUS:CONFIRMED,MODSINCE:%1,TYPE:ADIF").arg(since);
    const QByteArray body = buildBody(apiKey, QStringLiteral("FETCH"), option);

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

    if (m_reply->error() != QNetworkReply::NoError) {
        const QString err = tr("QRZ download error: %1").arg(m_reply->errorString());
        emit logMessage(err);
        emit downloadFinished({}, {err});
        m_reply = nullptr;
        return;
    }

    const QString body = QString::fromUtf8(m_reply->readAll());
    m_reply = nullptr;

    emit logMessage(tr("Received %1 byte(s) from QRZ.").arg(body.size()));
    emit logMessage(tr("Raw response prefix: %1").arg(body.left(400).trimmed()));

    // The QRZ response is nominally URL-encoded, but the DATA field contains raw
    // ADIF which itself has '&' and '=' characters — QUrlQuery mangles it.
    // Strategy: parse the small header fields (RESULT/REASON/COUNT) from the
    // portion before "DATA=", then extract DATA as everything after "DATA=".
    const QString dataMarker = QStringLiteral("DATA=");
    const int dataPos = body.indexOf(dataMarker);

    // Header portion (safe to parse with QUrlQuery — no ADIF yet)
    const QString header = (dataPos >= 0) ? body.left(dataPos) : body;
    QUrlQuery response(header);
    const QString result = response.queryItemValue(QStringLiteral("RESULT"));
    const QString reason = response.queryItemValue(QStringLiteral("REASON"));
    const QString count  = response.queryItemValue(QStringLiteral("COUNT"));

    if (result.compare(QLatin1String("OK"), Qt::CaseInsensitive) != 0) {
        // COUNT=0 / "No records" is not an error — just nothing new
        if (count == QLatin1String("0") ||
            reason.contains(QLatin1String("No records"), Qt::CaseInsensitive)) {
            emit logMessage(tr("Parsed 0 confirmation(s) from QRZ."));
            emit downloadFinished({}, {});
        } else {
            const QString err = tr("QRZ fetch error: %1").arg(reason.isEmpty() ? result : reason);
            emit logMessage(err);
            emit downloadFinished({}, {err});
        }
        return;
    }

    // Extract the raw ADIF — everything after "DATA="
    const QString adif = (dataPos >= 0)
                         ? QUrl::fromPercentEncoding(body.mid(dataPos + dataMarker.size()).toUtf8())
                         : QString();

    if (adif.isEmpty()) {
        emit logMessage(tr("Parsed 0 confirmation(s) from QRZ."));
        emit downloadFinished({}, {});
        return;
    }

    const AdifParser::Result parsed = AdifParser::parseString(adif);

    if (!parsed.warnings.isEmpty()) {
        emit logMessage(tr("Parser warnings:"));
        for (const QString &w : parsed.warnings)
            emit logMessage(tr("  %1").arg(w));
    }

    // STATUS:CONFIRMED means every returned QSO is confirmed by the other station.
    const QDate today = QDate::currentDate();
    QList<Qso> confirmed = parsed.qsos;
    for (Qso &q : confirmed) {
        q.qrzQslRcvd = 'Y';
        if (!q.qrzRcvdDate.isValid())
            q.qrzRcvdDate = today;
    }

    emit logMessage(tr("Parsed %1 confirmation(s) from QRZ:").arg(confirmed.size()));
    for (const Qso &q : confirmed) {
        emit logMessage(tr("  %1  %2  %3  %4")
                            .arg(q.callsign,
                                 q.datetimeOn.toUTC().toString(QStringLiteral("yyyy-MM-dd")),
                                 q.band, q.mode));
    }

    emit downloadFinished(confirmed, parsed.warnings);
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
