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
        emit uploadFinished({}, {tr("No unsent QSOs for eQSL.")});
        return;
    }

    const Settings &s    = Settings::instance();
    const QString   user = s.eqslUsername();
    const QString   pass = SecureSettings::instance().get(SecureKey::EQSL_PASSWORD);

    if (user.isEmpty() || pass.isEmpty()) {
        emit uploadFinished({}, {tr("eQSL credentials not configured.")});
        m_pendingUpload.clear();
        return;
    }

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

    QStringList errors;
    if (m_reply->error() != QNetworkReply::NoError) {
        errors << tr("eQSL upload error: %1").arg(m_reply->errorString());
        emit uploadFinished({}, errors);
        m_reply = nullptr;
        m_pendingUpload.clear();
        return;
    }

    const QString body = QString::fromUtf8(m_reply->readAll());
    m_reply = nullptr;

    // eQSL returns "Result: 0 records added" or an error description
    if (body.contains(QLatin1String("Error"), Qt::CaseInsensitive) &&
        !body.contains(QLatin1String("0 Duplicate"), Qt::CaseInsensitive)) {
        emit uploadFinished({}, {tr("eQSL error: %1").arg(body.trimmed())});
        m_pendingUpload.clear();
        return;
    }

    // Mark sent
    const QDate today = QDate::currentDate();
    for (Qso &q : m_pendingUpload) {
        q.eqslQslSent = 'Y';
        q.eqslSentDate = today;
    }

    emit uploadFinished(m_pendingUpload, {});
    m_pendingUpload.clear();
}

// ---------------------------------------------------------------------------
// Download
// ---------------------------------------------------------------------------

void EqslService::startDownload()
{
    const Settings &s    = Settings::instance();
    const QString   user = s.eqslUsername();
    const QString   pass = SecureSettings::instance().get(SecureKey::EQSL_PASSWORD);

    if (user.isEmpty() || pass.isEmpty()) {
        emit downloadFinished({}, {tr("eQSL credentials not configured.")});
        return;
    }

    // Date in MM/DD/YYYY format expected by eQSL
    const QDate   since  = QDate::currentDate().addDays(-90);
    const QString sinceStr = since.toString(QStringLiteral("MM/dd/yyyy"));

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

    QStringList errors;
    if (m_reply->error() != QNetworkReply::NoError) {
        errors << tr("eQSL download error: %1").arg(m_reply->errorString());
        emit downloadFinished({}, errors);
        m_reply = nullptr;
        return;
    }

    const QString body = QString::fromUtf8(m_reply->readAll());
    m_reply = nullptr;

    if (body.contains(QLatin1String("Error"), Qt::CaseInsensitive) &&
        !body.contains(QLatin1String("<call:"), Qt::CaseInsensitive)) {
        emit downloadFinished({}, {tr("eQSL download error: %1").arg(body.trimmed())});
        return;
    }

    const AdifParser::Result parsed = AdifParser::parseString(body);
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
