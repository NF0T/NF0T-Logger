// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include "SettingsPage.h"

class QRadioButton;

class DisplayPage : public SettingsPage
{
    Q_OBJECT

public:
    explicit DisplayPage(QWidget *parent = nullptr);

    QString pageTitle() const override { return tr("Display"); }
    void load()  override;
    void apply() override;

private:
    QRadioButton *m_imperial = nullptr;   // miles (default)
    QRadioButton *m_metric   = nullptr;   // km
};
