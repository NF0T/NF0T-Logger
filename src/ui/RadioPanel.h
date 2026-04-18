// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <QWidget>

class QLabel;

/// Compact bar showing RX/TX state and dial frequency.
///
/// RX/TX pills reflect hardware radio backend state only.
/// Frequency can be fed from either a RadioBackend or a DigitalListenerService.
class RadioPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RadioPanel(QWidget *parent = nullptr);

public slots:
    void setConnected(bool connected);
    void setTransmitting(bool transmitting);
    void setFrequency(double freqMhz);

private:
    void updatePills();
    static QString formatFreq(double freqMhz);

    QLabel *m_rxPill      = nullptr;
    QLabel *m_txPill      = nullptr;
    QLabel *m_freqDisplay = nullptr;

    bool   m_connected    = false;
    bool   m_transmitting = false;
    double m_freqMhz      = 0.0;
};
