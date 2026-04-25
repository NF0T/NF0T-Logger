// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <QDialog>
#include <QList>

class QListWidget;
class QStackedWidget;
class SettingsPage;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    enum Page { Station = 0, Equipment, Database, Radio, QslServices, CallsignLookup };

    explicit SettingsDialog(QWidget *parent = nullptr);

    void showPage(Page page);

private slots:
    void onApply();
    void onAccepted();

private:
    void applyAll();

    QListWidget    *m_nav    = nullptr;
    QStackedWidget *m_stack  = nullptr;
    QList<SettingsPage *> m_pages;
};
