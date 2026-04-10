#pragma once

#include "qsl/QslService.h"

class QNetworkAccessManager;
class QNetworkReply;

/// eQSL.cc QSL service.
///
/// Upload:   HTTP POST to ImportADIF.cfm with ADIF payload.
/// Download: HTTP GET to DownloadInBox.cfm, parses returned ADIF.
class EqslService : public QslService
{
    Q_OBJECT

public:
    explicit EqslService(QObject *parent = nullptr);
    ~EqslService() override;

    QString displayName() const override { return QStringLiteral("eQSL"); }
    bool    isEnabled()   const override;

    void startUpload  (const QList<Qso> &allQsos)          override;
    void startDownload(const QDate &from, const QDate &to) override;
    void abort()                                            override;

private slots:
    void onUploadReply();
    void onDownloadReply();

private:
    QNetworkAccessManager *m_nam   = nullptr;
    QNetworkReply         *m_reply = nullptr;

    QList<Qso> m_pendingUpload;
};
