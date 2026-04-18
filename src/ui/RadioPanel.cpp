// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "RadioPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Pill stylesheets
// ---------------------------------------------------------------------------

static const char *kPillDark =
    "QLabel { background: #1e1e1e; color: #484848;"
    " border: 1px solid #333; border-radius: 4px;"
    " padding: 1px 8px; font-weight: bold; font-size: 10px; }";

static const char *kPillRx =
    "QLabel { background: #0d3d1a; color: #00dd55;"
    " border: 1px solid #00aa44; border-radius: 4px;"
    " padding: 1px 8px; font-weight: bold; font-size: 10px; }";

static const char *kPillTx =
    "QLabel { background: #3d0d0d; color: #ff4444;"
    " border: 1px solid #cc2222; border-radius: 4px;"
    " padding: 1px 8px; font-weight: bold; font-size: 10px; }";

static const char *kFreqConnected =
    "QLabel { color: #00dd55; font-family: monospace;"
    " font-size: 26px; font-weight: bold; padding: 0 12px; }";

static const char *kFreqDisconnected =
    "QLabel { color: #484848; font-family: monospace;"
    " font-size: 26px; font-weight: bold; padding: 0 12px; }";

static const char *kPanelStyle =
    "RadioPanel { background: #141414; border-bottom: 1px solid #333; }";

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

RadioPanel::RadioPanel(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet(kPanelStyle);
    setFixedHeight(56);

    m_rxPill = new QLabel(tr("RX"), this);
    m_txPill = new QLabel(tr("TX"), this);
    m_rxPill->setAlignment(Qt::AlignCenter);
    m_txPill->setAlignment(Qt::AlignCenter);
    m_rxPill->setMinimumWidth(36);
    m_txPill->setMinimumWidth(36);

    auto *pillLayout = new QVBoxLayout;
    pillLayout->setContentsMargins(8, 6, 4, 6);
    pillLayout->setSpacing(4);
    pillLayout->addWidget(m_rxPill);
    pillLayout->addWidget(m_txPill);

    m_freqDisplay = new QLabel(formatFreq(0.0), this);
    m_freqDisplay->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addLayout(pillLayout);
    row->addWidget(m_freqDisplay);
    row->addStretch();

    updatePills();
}

// ---------------------------------------------------------------------------
// Public slots
// ---------------------------------------------------------------------------

void RadioPanel::setConnected(bool connected)
{
    if (m_connected == connected) return;
    m_connected = connected;
    if (!connected) {
        m_transmitting = false;
        m_freqMhz      = 0.0;
        m_freqDisplay->setText(formatFreq(0.0));
        m_freqDisplay->setStyleSheet(kFreqDisconnected);
    } else {
        m_freqDisplay->setStyleSheet(kFreqConnected);
    }
    updatePills();
}

void RadioPanel::setTransmitting(bool transmitting)
{
    if (m_transmitting == transmitting) return;
    m_transmitting = transmitting;
    updatePills();
}

void RadioPanel::setFrequency(double freqMhz)
{
    m_freqMhz = freqMhz;
    m_freqDisplay->setText(formatFreq(freqMhz));
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void RadioPanel::updatePills()
{
    if (!m_connected) {
        m_rxPill->setStyleSheet(kPillDark);
        m_txPill->setStyleSheet(kPillDark);
    } else if (m_transmitting) {
        m_rxPill->setStyleSheet(kPillDark);
        m_txPill->setStyleSheet(kPillTx);
    } else {
        m_rxPill->setStyleSheet(kPillRx);
        m_txPill->setStyleSheet(kPillDark);
    }
}

// static
QString RadioPanel::formatFreq(double freqMhz)
{
    if (freqMhz <= 0.0)
        return QStringLiteral("---.---.---");

    const qint64 totalHz = static_cast<qint64>(qRound(freqMhz * 1e6));
    const int mhz = static_cast<int>(totalHz / 1000000LL);
    const int khz = static_cast<int>((totalHz % 1000000LL) / 1000LL);
    const int hz  = static_cast<int>(totalHz % 1000LL);

    return QStringLiteral("%1.%2.%3")
        .arg(mhz, 3, 10, QChar('0'))
        .arg(khz, 3, 10, QChar('0'))
        .arg(hz,  3, 10, QChar('0'));
}
