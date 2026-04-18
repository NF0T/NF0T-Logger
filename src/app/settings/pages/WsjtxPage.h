// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include "SettingsPage.h"

class QCheckBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QSpinBox;

class WsjtxPage : public SettingsPage
{
    Q_OBJECT

public:
    explicit WsjtxPage(QWidget *parent = nullptr);

    QString pageTitle() const override { return tr("WSJT-X"); }
    void load()  override;
    void apply() override;

private slots:
    void onAddressChanged(const QString &text);

private:
    void populateIfaceList();

    QCheckBox  *m_enabled        = nullptr;
    QSpinBox   *m_port           = nullptr;
    QLineEdit  *m_multicastGroup = nullptr;
    QCheckBox  *m_autoLog        = nullptr;
    QGroupBox  *m_ifaceBox       = nullptr;
    QListWidget *m_ifaceList     = nullptr;
};
