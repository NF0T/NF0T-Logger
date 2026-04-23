// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <QDialog>

class QNetworkAccessManager;
class QTextBrowser;

class WhatsNewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WhatsNewDialog(const QString &version, QWidget *parent = nullptr);

private:
    QTextBrowser          *m_browser = nullptr;
    QNetworkAccessManager *m_nam     = nullptr;
    QString                m_version;
};
