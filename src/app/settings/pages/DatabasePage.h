// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include "SettingsPage.h"

class QGroupBox;
class QLineEdit;
class QSpinBox;
class QRadioButton;

class DatabasePage : public SettingsPage
{
    Q_OBJECT

public:
    explicit DatabasePage(QWidget *parent = nullptr);

    QString pageTitle() const override { return tr("Database"); }
    void load()  override;
    void apply() override;

private slots:
    void onBackendToggled();

private:
    QRadioButton *m_sqliteRadio   = nullptr;
    QRadioButton *m_mariadbRadio  = nullptr;

    // SQLite
    QGroupBox  *m_sqliteGroup   = nullptr;
    QLineEdit  *m_sqlitePath    = nullptr;

    // MariaDB
    QGroupBox  *m_mariadbGroup  = nullptr;
    QLineEdit  *m_dbHost        = nullptr;
    QSpinBox   *m_dbPort        = nullptr;
    QLineEdit  *m_dbDatabase    = nullptr;
    QLineEdit  *m_dbUsername    = nullptr;
    QLineEdit  *m_dbPassword    = nullptr;
};
