#include "EqslService.h"

#include <QDate>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
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
    Q_UNUSED(to)   // eQSL API has no end-date parameter

    const Settings &s    = Settings::instance();
    const QString   user = s.eqslUsername();
    const QString   pass = SecureSettings::instance().get(SecureKey::EQSL_PASSWORD);

    if (user.isEmpty() || pass.isEmpty()) {
        const QString err = tr("eQSL credentials not configured.");
        emit logMessage(err);
        emit downloadFinished({}, {err});
        return;
    }

    // eQSL expects MM/DD/YYYY
    const QString sinceStr = from.toString(QStringLiteral("MM/dd/yyyy"));
    emit logMessage(tr("Requesting eQSL inbox for %1 since %2...").arg(user, sinceStr));

    QUrl url = DOWNLOAD_URL;
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("UserName"),      user);
    q.addQueryItem(QStringLiteral("Password"),      pass);
    q.addQueryItem(QStringLiteral("RcvdSince"),     sinceStr);
    q.addQueryItem(QStringLiteral("ConfirmedOnly"), QStringLiteral("1"));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NF0T Logger/0.1"));

    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::finished, this, &EqslService::onDownloadReply);
}

void EqslService::onDownloadReply()
{
    m_reply->deleteLater();

    if (m_reply->error() != QNetworkReply::NoError) {
        const QString err = tr("eQSL download error: %1").arg(m_reply->errorString());
        emit logMessage(err);
        emit downloadFinished({}, {err});
        m_reply = nullptr;
        return;
    }

    const QString body = QString::fromUtf8(m_reply->readAll());
    m_reply = nullptr;

    emit logMessage(tr("Received %1 byte(s) from eQSL.").arg(body.size()));

    if (body.contains(QLatin1String("Error"), Qt::CaseInsensitive) &&
        !body.contains(QLatin1String("<call:"), Qt::CaseInsensitive)) {
        const QString err = tr("eQSL download error: %1").arg(body.trimmed());
        emit logMessage(err);
        emit downloadFinished({}, {err});
        return;
    }

    const AdifParser::Result parsed = AdifParser::parseString(body);
    emit logMessage(tr("Parsed %1 confirmation(s) from eQSL:").arg(parsed.qsos.size()));
    for (const Qso &q : parsed.qsos) {
        emit logMessage(tr("  %1  %2  %3  %4")
                            .arg(q.callsign,
                                 q.datetimeOn.toUTC().toString(QStringLiteral("yyyy-MM-dd")),
                                 q.band, q.mode));
    }

    emit downloadFinished(parsed.qsos, parsed.warnings);
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
