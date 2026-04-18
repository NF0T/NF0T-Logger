// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include "qsl/QslService.h"

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;
class QTemporaryFile;

/// ARRL Logbook of the World QSL service.
///
/// Upload:   Signs and uploads via the TQSL command-line tool.
/// Download: HTTP GET to lotw.arrl.org; parses returned ADIF.
class LoTwService : public QslService
{
    Q_OBJECT

public:
    explicit LoTwService(QObject *parent = nullptr);
    ~LoTwService() override;

    QString displayName() const override { return QStringLiteral("LoTW"); }
    bool    isEnabled()   const override;

    void startUpload  (const QList<Qso> &allQsos)          override;
    void startDownload(const QDate &from, const QDate &to) override;
    void abort()                                            override;

private slots:
    void onTqslOutput();
    void onTqslError();
    void onTqslFinished(int exitCode, int exitStatus);
    void onDownloadReply();

private:
    QNetworkAccessManager *m_nam      = nullptr;
    QNetworkReply         *m_reply    = nullptr;
    QProcess              *m_tqsl     = nullptr;
    QTemporaryFile        *m_tempFile = nullptr;

    QList<Qso> m_pendingUpload;
};
