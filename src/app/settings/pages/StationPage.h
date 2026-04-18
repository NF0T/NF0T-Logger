// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include "SettingsPage.h"

class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;

class StationPage : public SettingsPage
{
    Q_OBJECT

public:
    explicit StationPage(QWidget *parent = nullptr);

    QString pageTitle() const override { return tr("Station"); }
    void load()  override;
    void apply() override;

private:
    QLineEdit     *m_callsign   = nullptr;
    QLineEdit     *m_name       = nullptr;
    QLineEdit     *m_grid       = nullptr;
    QLineEdit     *m_city       = nullptr;
    QLineEdit     *m_state      = nullptr;
    QLineEdit     *m_country    = nullptr;
    QSpinBox      *m_dxcc       = nullptr;
    QSpinBox      *m_cqZone     = nullptr;
    QSpinBox      *m_ituZone    = nullptr;
    QDoubleSpinBox *m_lat       = nullptr;
    QDoubleSpinBox *m_lon       = nullptr;
};
