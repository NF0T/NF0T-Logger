// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include "SettingsPage.h"

class QLineEdit;
class QDoubleSpinBox;

class EquipmentPage : public SettingsPage
{
    Q_OBJECT

public:
    explicit EquipmentPage(QWidget *parent = nullptr);

    QString pageTitle() const override { return tr("Equipment"); }
    void load()  override;
    void apply() override;

private:
    QLineEdit      *m_rig     = nullptr;
    QLineEdit      *m_antenna = nullptr;
    QDoubleSpinBox *m_txPwr   = nullptr;
};
