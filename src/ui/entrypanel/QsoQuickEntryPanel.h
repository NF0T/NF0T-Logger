// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <QWidget>
#include "core/logbook/Qso.h"

class QComboBox;
class QDateTimeEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

/// Two-column quick QSO entry panel.
///
/// Left column: essential entry fields (datetime, callsign, band/freq, mode,
/// RST, comment) plus Log / Full Entry / Clear actions.
///
/// Right column: callsign lookup results and previous-QSO history — populated
/// by MainWindow via setPreviousQsos() and (in a later step) setLookupResult().
class QsoQuickEntryPanel : public QWidget
{
    Q_OBJECT

public:
    explicit QsoQuickEntryPanel(QWidget *parent = nullptr);

    void setRadioFreq(double freqMhz);
    void setRadioMode(const QString &mode, const QString &submode = {});
    void setDxCall(const QString &call);
    void setDxGrid(const QString &grid);

    // Called by MainWindow with DB results for the current callsign (step 7).
    void setPreviousQsos(const QList<Qso> &qsos, int total);

public slots:
    void clearForm();

signals:
    void qsoReady(const Qso &qso);
    void callsignChanged(const QString &callsign);
    void fullEntryRequested(const Qso &partialQso);

private slots:
    void onModeChanged(int index);
    void onFreqChanged(double freqMhz);
    void onBandChanged(int index);
    void onLogClicked();
    void onNowClicked();

private:
    Qso  buildQso() const;
    bool validate() const;
    void applyStationDefaults(Qso &qso) const;
    void updateRstDefaults();
    void setFreqSilently(double freqMhz);
    void setBandSilently(const QString &band);

    // Left column — entry fields
    QDateTimeEdit  *m_dateTime     = nullptr;
    QPushButton    *m_nowBtn       = nullptr;
    QLineEdit      *m_callsign     = nullptr;
    QComboBox      *m_band         = nullptr;
    QDoubleSpinBox *m_freq         = nullptr;
    QComboBox      *m_mode         = nullptr;
    QComboBox      *m_submode      = nullptr;
    QLineEdit      *m_rstSent      = nullptr;
    QLineEdit      *m_rstRcvd      = nullptr;
    QLineEdit      *m_comment      = nullptr;
    QPushButton    *m_fullEntryBtn = nullptr;
    QPushButton    *m_logBtn       = nullptr;
    QPushButton    *m_clearBtn     = nullptr;

    // Right column — context panel
    QLabel      *m_lookupLabel     = nullptr;
    QLabel      *m_prevQsosHeader  = nullptr;
    QListWidget *m_prevQsosList    = nullptr;

    // Grid from WSJT-X stationSelected; not shown in quick panel but included in QSO
    QString m_dxGrid;

    bool m_suppressBandFreqSignals = false;
};
