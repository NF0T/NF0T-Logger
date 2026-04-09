#pragma once

#include "qsl/QslService.h"

class QNetworkAccessManager;
class QNetworkReply;

/// ClubLog QSL upload service.
///
/// Upload: multipart/form-data POST to clublog.org/realtime.php.
/// ClubLog has no inbound QSL confirmation — canDownload() returns false
/// and startDownload() is a no-op.
class ClubLogService : public QslService
{
    Q_OBJECT

public:
    explicit ClubLogService(QObject *parent = nullptr);
    ~ClubLogService() override;

    QString displayName() const override { return QStringLiteral("ClubLog"); }
    bool    isEnabled()   const override;
    bool    canDownload() const override { return false; }

    void startUpload  (const QList<Qso> &allQsos) override;
    void startDownload()                           override {}   // no-op
    void abort()                                   override;

private slots:
    void onUploadReply();

private:
    QNetworkAccessManager *m_nam   = nullptr;
    QNetworkReply         *m_reply = nullptr;

    QList<Qso> m_pendingUpload;
};
