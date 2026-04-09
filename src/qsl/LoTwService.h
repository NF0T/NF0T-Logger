#pragma once

#include "qsl/QslService.h"

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;
class QTemporaryFile;

/// LoTW QSL service.
///
/// Upload:  writes a temp ADIF and calls TQSL via QProcess.
/// Download: HTTP GET to the ARRL LoTW report endpoint, then parses the
///           returned ADIF for received confirmations.
class LoTwService : public QslService
{
    Q_OBJECT

public:
    explicit LoTwService(QObject *parent = nullptr);
    ~LoTwService() override;

    QString displayName() const override { return QStringLiteral("LoTW"); }
    bool    isEnabled()   const override;

    void startUpload  (const QList<Qso> &allQsos) override;
    void startDownload()                           override;
    void abort()                                   override;

private slots:
    void onTqslFinished(int exitCode, int exitStatus);
    void onDownloadReply();

private:
    QNetworkAccessManager *m_nam      = nullptr;
    QNetworkReply         *m_reply    = nullptr;
    QProcess              *m_tqsl     = nullptr;
    QTemporaryFile        *m_tempFile = nullptr;

    QList<Qso> m_pendingUpload;   // QSOs being uploaded (to emit back on success)
};
