// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "CallsignLookupPage.h"
#include "app/settings/Settings.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

static QLineEdit *makePasswordField(QWidget *parent)
{
    auto *le = new QLineEdit(parent);
    le->setEchoMode(QLineEdit::Password);
    return le;
}

static QLabel *keychainNote(QWidget *parent)
{
    return new QLabel(QStringLiteral("<i>Stored securely in the system keychain.</i>"), parent);
}

CallsignLookupPage::CallsignLookupPage(QWidget *parent)
    : SettingsPage(parent)
{
    m_enabled        = new QCheckBox(tr("Enable callsign lookup"), this);
    m_qrzXmlUsername = new QLineEdit(this);
    m_qrzXmlPassword = makePasswordField(this);

    auto *form = new QFormLayout(this);
    form->addRow(m_enabled);
    form->addRow(new QLabel(QStringLiteral("<b>QRZ XML Data API</b>"), this));
    form->addRow(tr("Username:"), m_qrzXmlUsername);
    form->addRow(tr("Password:"), m_qrzXmlPassword);
    form->addRow(keychainNote(this));
    form->addRow(new QLabel(
        tr("Requires a QRZ.com account with XML data access (free for logged-in users)."), this));
}

void CallsignLookupPage::load()
{
    const Settings &s = Settings::instance();
    m_enabled->setChecked(s.callsignLookupEnabled());
    m_qrzXmlUsername->setText(s.qrzXmlUsername());
    m_qrzXmlPassword->setText(s.qrzXmlPassword());
}

void CallsignLookupPage::apply()
{
    Settings &s = Settings::instance();
    s.setCallsignLookupEnabled(m_enabled->isChecked());
    s.setQrzXmlUsername(m_qrzXmlUsername->text().trimmed());
    s.setQrzXmlPassword(m_qrzXmlPassword->text());
}
