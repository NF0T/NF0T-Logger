// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "LogFilterBar.h"

#include <QComboBox>
#include <QDateEdit>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTime>
#include <QTimeZone>
#include <QTimer>

#include "core/BandPlan.h"

static const QDate kNoDate(1900, 1, 1);

LogFilterBar::LogFilterBar(QWidget *parent)
    : QWidget(parent)
{
    m_callsign = new QLineEdit(this);
    m_callsign->setPlaceholderText(tr("Callsign"));
    m_callsign->setFixedWidth(110);
    m_callsign->setMaxLength(20);

    m_band = new QComboBox(this);
    m_band->setFixedWidth(82);
    m_band->addItem(tr("All bands"));
    m_band->addItems(BandPlan::bandNames());

    m_mode = new QComboBox(this);
    m_mode->setFixedWidth(82);
    m_mode->addItem(tr("All modes"));
    m_mode->addItems(BandPlan::modeNames());

    m_dateFrom = new QDateEdit(this);
    m_dateFrom->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_dateFrom->setCalendarPopup(true);
    m_dateFrom->setMinimumDate(kNoDate);
    m_dateFrom->setDate(kNoDate);
    m_dateFrom->setSpecialValueText(tr("From: any"));
    m_dateFrom->setFixedWidth(118);

    m_dateTo = new QDateEdit(this);
    m_dateTo->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_dateTo->setCalendarPopup(true);
    m_dateTo->setMinimumDate(kNoDate);
    m_dateTo->setDate(kNoDate);
    m_dateTo->setSpecialValueText(tr("To: any"));
    m_dateTo->setFixedWidth(108);

    m_qslStatus = new QComboBox(this);
    m_qslStatus->setFixedWidth(165);
    m_qslStatus->addItem(tr("All QSL states"));
    m_qslStatus->addItem(tr("LoTW — unsent"));
    m_qslStatus->addItem(tr("LoTW — pending"));
    m_qslStatus->addItem(tr("eQSL — unsent"));
    m_qslStatus->addItem(tr("eQSL — pending"));
    m_qslStatus->addItem(tr("QRZ — unsent"));
    m_qslStatus->addItem(tr("QRZ — pending"));
    m_qslStatus->addItem(tr("ClubLog — unsent"));

    m_clearBtn = new QPushButton(tr("Clear"), this);
    m_clearBtn->setFixedWidth(55);

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(300);

    auto makeSep = [this]() {
        auto *f = new QFrame(this);
        f->setFrameShape(QFrame::VLine);
        f->setFrameShadow(QFrame::Sunken);
        return f;
    };

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 3, 6, 3);
    layout->setSpacing(6);
    layout->addWidget(new QLabel(tr("Filter:"), this));
    layout->addWidget(m_callsign);
    layout->addWidget(makeSep());
    layout->addWidget(m_band);
    layout->addWidget(m_mode);
    layout->addWidget(makeSep());
    layout->addWidget(m_dateFrom);
    layout->addWidget(m_dateTo);
    layout->addWidget(makeSep());
    layout->addWidget(m_qslStatus);
    layout->addStretch();
    layout->addWidget(m_clearBtn);

    connect(m_callsign, &QLineEdit::textChanged,
            m_debounce, QOverload<>::of(&QTimer::start));
    connect(m_debounce, &QTimer::timeout,
            this, &LogFilterBar::emitFilter);

    connect(m_band,      QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LogFilterBar::emitFilter);
    connect(m_mode,      QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LogFilterBar::emitFilter);
    connect(m_dateFrom,  &QDateEdit::dateChanged,
            this, &LogFilterBar::emitFilter);
    connect(m_dateTo,    &QDateEdit::dateChanged,
            this, &LogFilterBar::emitFilter);
    connect(m_qslStatus, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LogFilterBar::emitFilter);
    connect(m_clearBtn,  &QPushButton::clicked,
            this, &LogFilterBar::reset);
}

QsoFilter LogFilterBar::currentFilter() const
{
    QsoFilter f;

    const QString call = m_callsign->text().trimmed().toUpper();
    if (!call.isEmpty())
        f.call = call;

    if (m_band->currentIndex() > 0)
        f.band = m_band->currentText();

    if (m_mode->currentIndex() > 0)
        f.mode = m_mode->currentText();

    if (m_dateFrom->date() != kNoDate)
        f.from = QDateTime(m_dateFrom->date(), QTime(0, 0, 0), QTimeZone::utc());

    if (m_dateTo->date() != kNoDate)
        f.to = QDateTime(m_dateTo->date(), QTime(23, 59, 59), QTimeZone::utc());

    switch (m_qslStatus->currentIndex()) {
    case 1: f.lotwUnsent     = true; break;
    case 2: f.lotwPending    = true; break;
    case 3: f.eqslUnsent     = true; break;
    case 4: f.eqslPending    = true; break;
    case 5: f.qrzUnsent      = true; break;
    case 6: f.qrzPending     = true; break;
    case 7: f.clublogUnsent  = true; break;
    default: break;
    }

    return f;
}

bool LogFilterBar::isFiltered() const
{
    const QsoFilter f = currentFilter();
    return f.call.has_value()
        || f.band.has_value()
        || f.mode.has_value()
        || f.from.has_value()
        || f.to.has_value()
        || f.lotwUnsent.has_value()
        || f.lotwPending.has_value()
        || f.eqslUnsent.has_value()
        || f.eqslPending.has_value()
        || f.qrzUnsent.has_value()
        || f.qrzPending.has_value()
        || f.clublogUnsent.has_value();
}

void LogFilterBar::reset()
{
    m_debounce->stop();
    const QSignalBlocker b1(m_callsign);
    const QSignalBlocker b2(m_band);
    const QSignalBlocker b3(m_mode);
    const QSignalBlocker b4(m_dateFrom);
    const QSignalBlocker b5(m_dateTo);
    const QSignalBlocker b6(m_qslStatus);
    m_callsign->clear();
    m_band->setCurrentIndex(0);
    m_mode->setCurrentIndex(0);
    m_dateFrom->setDate(kNoDate);
    m_dateTo->setDate(kNoDate);
    m_qslStatus->setCurrentIndex(0);
    emitFilter();
}

void LogFilterBar::emitFilter()
{
    emit filterChanged(currentFilter());
}
