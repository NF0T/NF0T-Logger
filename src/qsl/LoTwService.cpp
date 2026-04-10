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
        emit logMessage(tr("No unsent QSOs for LoTW."));
        emit uploadFinished({}, {tr("No unsent QSOs for LoTW.")});
        return;
    }

    emit logMessage(tr("Preparing %1 QSO(s) for LoTW upload...").arg(m_pendingUpload.size()));

    // Write temp ADIF
    m_tempFile = new QTemporaryFile(QStringLiteral("nf0t_lotw_XXXXXX.adi"), this);
    m_tempFile->setAutoRemove(true);
    if (!m_tempFile->open()) {
        const QString err = tr("Could not create temporary ADIF file for TQSL.");
        emit logMessage(err);
        emit uploadFinished({}, {err});
        m_pendingUpload.clear();
        return;
    }

    const QString adif = AdifWriter::writeString(m_pendingUpload);
    m_tempFile->write(adif.toUtf8());
    m_tempFile->flush();
    const QString tempPath = m_tempFile->fileName();
    m_tempFile->close();

    // Build TQSL command
    const QString tqslPath        = Settings::instance().lotwTqslPath();
    const QString stationLocation = Settings::instance().lotwStationLocation();

    if (stationLocation.isEmpty()) {
        const QString err = tr("LoTW station location not set. "
                               "Enter the exact name from TQSL → Station Locations in Settings → QSL Services → LoTW.");
        emit logMessage(err);
        emit uploadFinished({}, {err});
        m_pendingUpload.clear();
        return;
    }

    QStringList args;
    args << QStringLiteral("-x")           // batch mode: exit after processing
         << QStringLiteral("-u")           // upload to LoTW after signing
         << QStringLiteral("-a") << QStringLiteral("compliant")
         << QStringLiteral("-l") << stationLocation
         << tempPath;

    const QString exe = tqslPath.isEmpty() ? QStringLiteral("tqsl") : tqslPath;
    emit logMessage(tr("Starting TQSL: %1 %2").arg(exe, args.join(QLatin1Char(' '))));

    m_tqsl = new QProcess(this);
    connect(m_tqsl, &QProcess::readyReadStandardOutput, this, &LoTwService::onTqslOutput);
    connect(m_tqsl, &QProcess::readyReadStandardError,  this, &LoTwService::onTqslError);
    connect(m_tqsl, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LoTwService::onTqslFinished);

    emit uploadProgress(0, 0);   // indeterminate
    m_tqsl->start(exe, args);
}

void LoTwService::onTqslOutput()
{
    const QString text = QString::fromLocal8Bit(m_tqsl->readAllStandardOutput()).trimmed();
    if (!text.isEmpty())
        emit logMessage(text);
}

void LoTwService::onTqslError()
{
    const QString text = QString::fromLocal8Bit(m_tqsl->readAllStandardError()).trimmed();
    if (!text.isEmpty())
        emit logMessage(text);
}

void LoTwService::onTqslFinished(int exitCode, int /*exitStatus*/)
{
    // Drain any remaining output not yet read via readyRead
    const QString remainOut = QString::fromLocal8Bit(m_tqsl->readAllStandardOutput()).trimmed();
    const QString remainErr = QString::fromLocal8Bit(m_tqsl->readAllStandardError()).trimmed();
    if (!remainOut.isEmpty()) emit logMessage(remainOut);
    if (!remainErr.isEmpty()) emit logMessage(remainErr);

    m_tqsl->deleteLater();
    m_tqsl = nullptr;

    if (m_tempFile) {
        m_tempFile->deleteLater();
        m_tempFile = nullptr;
    }

    emit logMessage(tr("TQSL exited with code %1.").arg(exitCode));

    if (exitCode != 0) {
        emit uploadFinished({}, {tr("TQSL exited with code %1.").arg(exitCode)});
        m_pendingUpload.clear();
        return;
    }

    // Mark sent
    const QDate today = QDate::currentDate();
    for (Qso &q : m_pendingUpload) {
        q.lotwQslSent = 'Y';
        q.lotwSentDate = today;
    }

    emit logMessage(tr("Successfully uploaded %1 QSO(s) to LoTW.").arg(m_pendingUpload.size()));
    emit uploadFinished(m_pendingUpload, {});
    m_pendingUpload.clear();
}

// ---------------------------------------------------------------------------
// Download from ARRL LoTW
// ---------------------------------------------------------------------------

void LoTwService::startDownload(const QDate &from, const QDate &to)
{
    Q_UNUSED(to)   // LoTW API has no end-date parameter; returns all since 'from'

    const Settings &s  = Settings::instance();
    const QString user = s.lotwCallsign().isEmpty() ? s.stationCallsign() : s.lotwCallsign();
    const QString pass = SecureSettings::instance().get(SecureKey::LOTW_PASSWORD);

    if (user.isEmpty() || pass.isEmpty()) {
        const QString err = tr("LoTW credentials not configured.");
        emit logMessage(err);
        emit downloadFinished({}, {err});
        return;
    }

    const QString since = from.toString(Qt::ISODate);
    emit logMessage(tr("Requesting LoTW confirmations for %1 since %2...").arg(user, since));

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("login"),        user);
    q.addQueryItem(QStringLiteral("password"),     pass);
    q.addQueryItem(QStringLiteral("qso_query"),    QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("qso_owncall"),  s.stationCallsign());
    q.addQueryItem(QStringLiteral("qso_qslsince"), since);
    q.addQueryItem(QStringLiteral("qso_qsldetail"),QStringLiteral("yes"));

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

    if (m_reply->error() != QNetworkReply::NoError) {
        const QString err = tr("LoTW download error: %1").arg(m_reply->errorString());
        emit logMessage(err);
        emit downloadFinished({}, {err});
        m_reply = nullptr;
        return;
    }

    const QString body = QString::fromUtf8(m_reply->readAll());
    m_reply = nullptr;

    emit logMessage(tr("Received %1 byte(s) from LoTW.").arg(body.size()));

    // Check for LoTW login error (returns HTML or "Login failed")
    if (body.contains(QLatin1String("Login Failed"), Qt::CaseInsensitive) ||
        body.contains(QLatin1String("<html"), Qt::CaseInsensitive)) {
        const QString err = tr("LoTW login failed. Check credentials.");
        emit logMessage(err);
        emit downloadFinished({}, {err});
        return;
    }

    const AdifParser::Result parsed = AdifParser::parseString(body);
    emit logMessage(tr("Parsed %1 confirmation(s) from LoTW:").arg(parsed.qsos.size()));
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

void LoTwService::abort()
{
    if (m_tqsl && m_tqsl->state() != QProcess::NotRunning)
        m_tqsl->kill();
    if (m_reply)
        m_reply->abort();
    m_pendingUpload.clear();
}
