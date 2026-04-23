// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "WhatsNewDialog.h"

#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTextBrowser>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

WhatsNewDialog::WhatsNewDialog(const QString &version, QWidget *parent)
    : QDialog(parent)
    , m_version(version)
{
    setWindowTitle(tr("What's New in NF0T Logger v%1").arg(version));
    setMinimumSize(560, 400);
    resize(680, 500);

    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(true);
    m_browser->setPlainText(tr("Fetching release notes…"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_browser);
    layout->addWidget(buttons);

    m_nam = new QNetworkAccessManager(this);

    QNetworkRequest req(QUrl(
        QStringLiteral("https://api.github.com/repos/NF0T/NF0T-Logger/releases/tags/v%1")
        .arg(version)));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("NF0T-Logger/%1").arg(version));
    req.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply *reply = m_nam->get(req);

    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->start(8000);
    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_browser->setPlainText(
                tr("Release notes for v%1 are not available.\n\n"
                   "Visit github.com/NF0T/NF0T-Logger/releases for details.")
                   .arg(m_version));
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QString body = obj.value(QStringLiteral("body")).toString().trimmed();
        if (body.isEmpty()) {
            m_browser->setPlainText(tr("No release notes available for v%1.").arg(m_version));
            return;
        }
        m_browser->setMarkdown(body);
    });
}
