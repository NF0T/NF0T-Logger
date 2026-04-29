// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include "SettingsPage.h"

class QCheckBox;
class QLineEdit;

class CallsignLookupPage : public SettingsPage
{
    Q_OBJECT

public:
    explicit CallsignLookupPage(QWidget *parent = nullptr);

    QString pageTitle() const override { return tr("Callsign Lookup"); }
    void load()  override;
    void apply() override;

private:
    QCheckBox *m_enabled        = nullptr;
    QLineEdit *m_qrzXmlUsername = nullptr;
    QLineEdit *m_qrzXmlPassword = nullptr;
};
