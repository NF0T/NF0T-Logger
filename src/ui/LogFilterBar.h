// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <QWidget>
#include "core/logbook/QsoFilter.h"

class QComboBox;
class QDateEdit;
class QLineEdit;
class QPushButton;
class QTimer;

class LogFilterBar : public QWidget
{
    Q_OBJECT

public:
    explicit LogFilterBar(QWidget *parent = nullptr);

    QsoFilter currentFilter() const;
    bool      isFiltered()    const;

public slots:
    void reset();

signals:
    void filterChanged(const QsoFilter &filter);

private:
    void emitFilter();

    QLineEdit   *m_callsign  = nullptr;
    QComboBox   *m_band      = nullptr;
    QComboBox   *m_mode      = nullptr;
    QDateEdit   *m_dateFrom  = nullptr;
    QDateEdit   *m_dateTo    = nullptr;
    QComboBox   *m_qslStatus = nullptr;
    QPushButton *m_clearBtn  = nullptr;
    QTimer      *m_debounce  = nullptr;
};
