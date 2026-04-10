#include "LoTwService.h"

#include <QDate>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QTemporaryFile>
#include <QUrl>
#include <QUrlQuery>

#include "app/settings/SecureSettings.h"
#include "app/settings/Settings.h"
#include "core/adif/AdifParser.h"
#include "core/adif/AdifWriter.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

LoTwService::LoTwService(QObject *parent)
    : QslService(parent)
    , m_nam(new QNetworkAccessManager(this))
{}

LoTwService::~LoTwService()
{
    abort();
}

bool LoTwService::isEnabled() const
{
    return Settings::instance().lotwEnabled();
}

// ---------------------------------------------------------------------------
// Upload via TQSL
// ---------------------------------------------------------------------------

void LoTwService::startUpload(const QList<Qso> &allQsos)
{
    // Filter to unsent
    m_pendingUpload.clear();
    for (const Qso &q : allQsos) {
        if (q.lotwQslSent != 'Y')
            m_pendingUpload.append(q);
    }

    if (m_pendingUpload.isEmpty()) {
        emit uploadFinished({}, {tr("No unsent QSOs for LoTW.")});
        return;
    }

    // Write temp ADIF
    m_tempFile = new QTemporaryFile(QStringLiteral("nf0t_lotw_XXXXXX.adi"), this);
    m_tempFile->setAutoRemove(true);
    if (!m_tempFile->open()) {
        emit uploadFinished({}, {tr("Could not create temporary ADIF file for TQSL.")});
        m_pendingUpload.clear();
        return;
    }

    const QString adif = AdifWriter::writeString(m_pendingUpload);
    m_tempFile->write(adif.toUtf8());
    m_tempFile->flush();
    const QString tempPath = m_tempFile->fileName();
    m_tempFile->close();

    // Build TQSL command
    const QString tqslPath       = Settings::instance().lotwTqslPath();
    const QString stationLocation = Settings::instance().lotwStationLocation();

    if (stationLocation.isEmpty()) {
        emit uploadFinished({}, {tr("LoTW station location not set. "
                                    "Enter the exact name from TQSL → Station Locations in Settings → QSL Services → LoTW.")});
        m_pendingUpload.clear();
        return;
    }

    QStringList args;
    args << QStringLiteral("-x")           // non-interactive
         << QStringLiteral("-a") << QStringLiteral("compliant")
         << QStringLiteral("-l") << stationLocation
         << tempPath;

    m_tqsl = new QProcess(this);
    connect(m_tqsl, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LoTwService::onTqslFinished);

    emit uploadProgress(0, 0);   // indeterminate
    m_tqsl->start(tqslPath.isEmpty() ? QStringLiteral("tqsl") : tqslPath, args);
}

void LoTwService::onTqslFinished(int exitCode, int /*exitStatus*/)
{
    const QString stdErr = QString::fromLocal8Bit(m_tqsl->readAllStandardError());
    m_tqsl->deleteLater();
    m_tqsl = nullptr;

    if (m_tempFile) {
        m_tempFile->deleteLater();
        m_tempFile = nullptr;
    }

    if (exitCode != 0) {
        QStringList errors;
        errors << tr("TQSL exited with code %1.").arg(exitCode);
        if (!stdErr.isEmpty())
            errors << stdErr;
        emit uploadFinished({}, errors);
        m_pendingUpload.clear();
        return;
    }

    // Mark sent
    const QDate today = QDate::currentDate();
    for (Qso &q : m_pendingUpload) {
        q.lotwQslSent = 'Y';
        q.lotwSentDate = today;
    }

    QStringList notes;
    if (!stdErr.isEmpty())
        notes << stdErr;

    emit uploadFinished(m_pendingUpload, notes);
    m_pendingUpload.clear();
}

// ---------------------------------------------------------------------------
// Download from ARRL LoTW
// ---------------------------------------------------------------------------

void LoTwService::startDownload()
{
    const Settings &s  = Settings::instance();
    const QString user = s.lotwCallsign().isEmpty() ? s.stationCallsign() : s.lotwCallsign();
    const QString pass = SecureSettings::instance().get(SecureKey::LOTW_PASSWORD);

    if (user.isEmpty() || pass.isEmpty()) {
        emit downloadFinished({}, {tr("LoTW credentials not configured.")});
        return;
    }

    // Fetch confirmations from the last 90 days
    const QString since = QDate::currentDate().addDays(-90).toString(Qt::ISODate);

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("login"),          user);
    q.addQueryItem(QStringLiteral("password"),        pass);
    q.addQueryItem(QStringLiteral("qso_query"),       QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("qso_owncall"),     s.stationCallsign());
    q.addQueryItem(QStringLiteral("qso_qslsince"),    since);
    q.addQueryItem(QStringLiteral("qso_qsldetail"),   QStringLiteral("yes"));

    QUrl url(QStringLiteral("https://lotw.arrl.org/lotwuser/lotwreport.adi"));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NF0T Logger/0.1"));

    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::finished, this, &LoTwService::onDownloadReply);
}

void LoTwService::onDownloadReply()
{
    m_reply->deleteLater();

    QStringList errors;
    if (m_reply->error() != QNetworkReply::NoError) {
        errors << tr("LoTW download error: %1").arg(m_reply->errorString());
        emit downloadFinished({}, errors);
        m_reply = nullptr;
        return;
    }

    const QString body = QString::fromUtf8(m_reply->readAll());
    m_reply = nullptr;

    // Check for LoTW login error (returns HTML or "Login failed")
    if (body.contains(QLatin1String("Login Failed"), Qt::CaseInsensitive) ||
        body.contains(QLatin1String("<html"), Qt::CaseInsensitive)) {
        emit downloadFinished({}, {tr("LoTW login failed. Check credentials.")});
        return;
    }

    const AdifParser::Result parsed = AdifParser::parseString(body);
    errors = parsed.warnings;

    // Each parsed QSO stub has callsign / datetimeOn / band / mode
    // and lotwQslRcvd/lotwQslRcvdDate filled in from ADIF LOTW_QSLRDATE field
    emit downloadFinished(parsed.qsos, errors);
}

// ---------------------------------------------------------------------------
// Abort
// ---------------------------------------------------------------------------

void LoTwService::abort()
{
    if (m_tqsl && m_tqsl->state() != QProcess::NotRunning)
        m_tqsl->kill();
    if (m_reply)
        m_reply->abort();
    m_pendingUpload.clear();
}
