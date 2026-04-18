// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <QWidget>

/// Abstract base for every settings page shown in SettingsDialog.
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr) : QWidget(parent) {}

    virtual QString pageTitle() const = 0;
    virtual void load()  = 0;   ///< Populate widgets from Settings.
    virtual void apply() = 0;   ///< Write widget values back to Settings.
};
