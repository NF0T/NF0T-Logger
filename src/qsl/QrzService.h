#pragma once

#include "qsl/QslService.h"

class QNetworkAccessManager;
class QNetworkReply;

/// QRZ Logbook QSL service.
///
/// Uses the QRZ Logbook API (https://logbook.qrz.com/api).
/// Upload:   POST with ACTION=INSERT and ADIF payload.
/// Download: POST with ACTION=FETCH, response is ADIF.
class QrzService : public QslService
{
    Q_OBJECT

public:
    explicit QrzService(QObject *parent = nullptr);
    ~QrzService() override;

    QString displayName() const override { return QStringLiteral("QRZ"); }
    bool    isEnabled()   const override;

    void startUpload  (const QList<Qso> &allQsos)          override;
    void startDownload(const QDate &from, const QDate &to) override;
    void abort()                                            override;

private slots:
    void onUploadReply();
    void onDownloadReply();

private:
    static QByteArray buildBody(const QString &apiKey, const QString &action,
                                const QString &option = {}, const QString &adif = {});

    QNetworkAccessManager *m_nam   = nullptr;
    QNetworkReply         *m_reply = nullptr;

    QList<Qso> m_pendingUpload;
};
