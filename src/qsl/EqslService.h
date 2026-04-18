// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include "qsl/QslService.h"

class QNetworkAccessManager;
class QNetworkReply;

/// eQSL.cc QSL service.
///
/// Upload:   HTTP POST to ImportADIF.cfm with ADIF payload.
/// Download: Two-step process per the eQSL API spec:
///   1. GET DownloadInBox.cfm — returns an HTML page with a link to the built ADIF file.
///   2. GET the linked .adi file — the actual ADIF data to parse.
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
    void onDownloadReply();   // step 1: parse HTML index page, extract ADIF link
    void onAdifFileReply();   // step 2: fetch and parse the actual ADIF file

private:
    QNetworkAccessManager *m_nam   = nullptr;
    QNetworkReply         *m_reply = nullptr;

    QList<Qso> m_pendingUpload;
};
