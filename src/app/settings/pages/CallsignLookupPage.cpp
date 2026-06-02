// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "CallsignLookupPage.h"
#include "app/settings/Settings.h"

#include <QCheckBox>
#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

static QLineEdit *makePasswordField(QWidget *parent)
{
    auto *le = new QLineEdit(parent);
    le->setEchoMode(QLineEdit::Password);
    return le;
}

CallsignLookupPage::CallsignLookupPage(QWidget *parent)
    : SettingsPage(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    // --- QRZ XML group ---
    auto *qrzGroup = new QGroupBox(tr("QRZ XML Data API"), this);
    m_qrzEnabled     = new QCheckBox(tr("Enable QRZ XML lookup"), qrzGroup);
    m_qrzXmlUsername = new QLineEdit(qrzGroup);
    m_qrzXmlPassword = makePasswordField(qrzGroup);

    auto *qrzForm = new QFormLayout(qrzGroup);
    qrzForm->addRow(m_qrzEnabled);
    qrzForm->addRow(tr("Username:"), m_qrzXmlUsername);
    qrzForm->addRow(tr("Password:"), m_qrzXmlPassword);
    qrzForm->addRow(new QLabel(
        QStringLiteral("<i>Stored securely in the system keychain.</i>"), qrzGroup));
    qrzForm->addRow(new QLabel(
        tr("Requires a QRZ.com account with XML data access."), qrzGroup));

    // --- CTY.dat group ---
    auto *ctyGroup = new QGroupBox(tr("CTY.dat (Offline DXCC)"), this);
    m_ctyEnabled     = new QCheckBox(tr("Enable CTY.dat offline lookup"), ctyGroup);
    m_ctyStatusLabel = new QLabel(ctyGroup);
    m_ctyUpdateBtn   = new QPushButton(tr("Update CTY.dat…"), ctyGroup);
    m_ctyUpdateBtn->setFixedWidth(160);

    connect(m_ctyUpdateBtn, &QPushButton::clicked,
            this, &CallsignLookupPage::onUpdateCtyDat);

    auto *ctyForm = new QFormLayout(ctyGroup);
    ctyForm->addRow(m_ctyEnabled);
    ctyForm->addRow(tr("File:"), m_ctyStatusLabel);
    ctyForm->addRow(m_ctyUpdateBtn);
    ctyForm->addRow(new QLabel(
        tr("Provides instant offline DXCC/zone/continent lookup. "
           "No subscription required."), ctyGroup));

    auto *vbox = new QVBoxLayout(this);
    vbox->addWidget(qrzGroup);
    vbox->addWidget(ctyGroup);
    vbox->addStretch();
}

void CallsignLookupPage::load()
{
    const Settings &s = Settings::instance();
    m_qrzEnabled->setChecked(s.callsignLookupEnabled());
    m_qrzXmlUsername->setText(s.qrzXmlUsername());
    m_qrzXmlPassword->setText(s.qrzXmlPassword());

    m_ctyEnabled->setChecked(s.ctyDatEnabled());
    refreshCtyStatus();
}

void CallsignLookupPage::apply()
{
    Settings &s = Settings::instance();
    s.setCallsignLookupEnabled(m_qrzEnabled->isChecked());
    s.setQrzXmlUsername(m_qrzXmlUsername->text().trimmed());
    s.setQrzXmlPassword(m_qrzXmlPassword->text());

    s.setCtyDatEnabled(m_ctyEnabled->isChecked());
}

void CallsignLookupPage::refreshCtyStatus()
{
    const QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString installed = dataDir + QStringLiteral("/cty.dat");

    if (QFileInfo::exists(installed)) {
        const QFileInfo fi(installed);
        m_ctyStatusLabel->setText(
            tr("Installed — updated %1")
                .arg(fi.lastModified().toString(QStringLiteral("yyyy-MM-dd"))));
    } else {
        m_ctyStatusLabel->setText(tr("Using bundled file (click Update to get the latest)"));
    }
}

void CallsignLookupPage::onUpdateCtyDat()
{
    m_ctyUpdateBtn->setEnabled(false);
    m_ctyStatusLabel->setText(tr("Downloading…"));

    QNetworkRequest req(QUrl(QStringLiteral("https://www.country-files.com/bigcty/cty.dat")));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("NF0T-Logger/" APP_VERSION));

    auto *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_ctyUpdateBtn->setEnabled(true);

        if (reply->error() != QNetworkReply::NoError) {
            m_ctyStatusLabel->setText(
                tr("Download failed: %1").arg(reply->errorString()));
            return;
        }

        const QByteArray data = reply->readAll();

        // Validate: must be plain text starting with a CTY.dat entity header
        // (not an HTML error page). Check the first non-empty line for 7+ colons.
        const QString text = QString::fromLatin1(data);
        bool valid = false;
        for (const QStringView line : QStringView(text).split(QLatin1Char('\n'))) {
            const QStringView trimmed = line.trimmed();
            if (!trimmed.isEmpty()) {
                valid = (trimmed.count(QLatin1Char(':')) >= 7);
                break;
            }
        }

        if (!valid) {
            m_ctyStatusLabel->setText(tr("Invalid file received — update aborted."));
            return;
        }

        const QString dataDir =
            QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        QDir().mkpath(dataDir);

        QFile f(dataDir + QStringLiteral("/cty.dat"));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            m_ctyStatusLabel->setText(
                tr("Could not write file: %1").arg(f.errorString()));
            return;
        }
        f.write(data);
        f.close();

        m_ctyStatusLabel->setText(
            tr("Updated — %1 (restart to apply)")
                .arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))));
    });
}
