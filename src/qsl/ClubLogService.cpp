#include "ClubLogService.h"

#include <QDate>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "app/settings/SecureSettings.h"
#include "app/settings/Settings.h"
#include "core/adif/AdifWriter.h"

static const QUrl UPLOAD_URL = QUrl(QStringLiteral("https://clublog.org/realtime.php"));

ClubLogService::ClubLogService(QObject *parent)
    : QslService(parent)
    , m_nam(new QNetworkAccessManager(this))
{}

ClubLogService::~ClubLogService()
{
    abort();
}

bool ClubLogService::isEnabled() const
{
    return Settings::instance().clublogEnabled();
}

// ---------------------------------------------------------------------------
// Upload
// ---------------------------------------------------------------------------

void ClubLogService::startUpload(const QList<Qso> &allQsos)
{
    m_pendingUpload.clear();
    for (const Qso &q : allQsos) {
        if (q.clublogQslSent != 'Y')
            m_pendingUpload.append(q);
    }

    if (m_pendingUpload.isEmpty()) {
        emit logMessage(tr("No unsent QSOs for ClubLog."));
        emit uploadFinished({}, {tr("No unsent QSOs for ClubLog.")});
        return;
    }

    const Settings &s      = Settings::instance();
    const QString   email  = s.clublogEmail();
    const QString   cs     = s.clublogCallsign().isEmpty() ? s.stationCallsign() : s.clublogCallsign();
    const QString   pass   = SecureSettings::instance().get(SecureKey::CLUBLOG_PASSWORD);
    const QString   appKey = SecureSettings::instance().get(SecureKey::CLUBLOG_APP_KEY);

    if (email.isEmpty() || pass.isEmpty() || appKey.isEmpty()) {
        const QString err = tr("ClubLog credentials not configured.");
        emit logMessage(err);
        emit uploadFinished({}, {err});
        m_pendingUpload.clear();
        return;
    }

    emit logMessage(tr("Uploading %1 QSO(s) to ClubLog as %2...").arg(m_pendingUpload.size()).arg(cs));

    AdifWriterOptions opts;
    opts.includeAppFields = false;
    const QByteArray adifBytes = AdifWriter::writeString(m_pendingUpload, opts).toUtf8();

    // ClubLog requires multipart/form-data
    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    auto addField = [&](const QString &name, const QByteArray &value) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"%1\"").arg(name));
        part.setBody(value);
        multiPart->append(part);
    };

    addField(QStringLiteral("email"),    email.toUtf8());
    addField(QStringLiteral("password"), pass.toUtf8());
    addField(QStringLiteral("callsign"), cs.toUtf8());
    addField(QStringLiteral("appkey"),   appKey.toUtf8());

    // ADIF part — ClubLog expects the file field named "adif"
    QHttpPart adifPart;
    adifPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"adif\"; filename=\"log.adi\""));
    adifPart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QStringLiteral("text/plain"));
    adifPart.setBody(adifBytes);
    multiPart->append(adifPart);

    QNetworkRequest req(UPLOAD_URL);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NF0T Logger/0.1"));

    emit uploadProgress(0, 0);
    m_reply = m_nam->post(req, multiPart);
    multiPart->setParent(m_reply);   // auto-delete with reply
    connect(m_reply, &QNetworkReply::finished, this, &ClubLogService::onUploadReply);
}

void ClubLogService::onUploadReply()
{
    m_reply->deleteLater();

    QStringList errors;
    if (m_reply->error() != QNetworkReply::NoError) {
        const QString err = tr("ClubLog upload error: %1").arg(m_reply->errorString());
        emit logMessage(err);
        emit uploadFinished({}, {err});
        m_reply = nullptr;
        m_pendingUpload.clear();
        return;
    }

    const QString body = QString::fromUtf8(m_reply->readAll()).trimmed();
    m_reply = nullptr;

    emit logMessage(tr("ClubLog response: %1").arg(body.left(200)));

    // ClubLog returns "OK" on success or an error message
    if (!body.startsWith(QLatin1String("OK"), Qt::CaseInsensitive)) {
        const QString err = tr("ClubLog error: %1").arg(body);
        emit logMessage(err);
        emit uploadFinished({}, {err});
        m_pendingUpload.clear();
        return;
    }

    const QDate today = QDate::currentDate();
    for (Qso &q : m_pendingUpload) {
        q.clublogQslSent = 'Y';
        q.clublogSentDate = today;
    }

    emit logMessage(tr("Successfully uploaded %1 QSO(s) to ClubLog.").arg(m_pendingUpload.size()));
    emit uploadFinished(m_pendingUpload, {});
    m_pendingUpload.clear();
}

// ---------------------------------------------------------------------------
// Abort
// ---------------------------------------------------------------------------

void ClubLogService::abort()
{
    if (m_reply)
        m_reply->abort();
    m_pendingUpload.clear();
}
