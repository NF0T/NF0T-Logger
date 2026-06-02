// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include "SettingsPage.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QPushButton;

class CallsignLookupPage : public SettingsPage
{
    Q_OBJECT

public:
    explicit CallsignLookupPage(QWidget *parent = nullptr);

    QString pageTitle() const override { return tr("Callsign Lookup"); }
    void load()  override;
    void apply() override;

private slots:
    void onUpdateCtyDat();

private:
    void refreshCtyStatus();

    // QRZ XML
    QCheckBox *m_qrzEnabled     = nullptr;
    QLineEdit *m_qrzXmlUsername = nullptr;
    QLineEdit *m_qrzXmlPassword = nullptr;

    // CTY.dat
    QCheckBox  *m_ctyEnabled     = nullptr;
    QLabel     *m_ctyStatusLabel = nullptr;
    QPushButton *m_ctyUpdateBtn  = nullptr;

    QNetworkAccessManager *m_nam = nullptr;
};
