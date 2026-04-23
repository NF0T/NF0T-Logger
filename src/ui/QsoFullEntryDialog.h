// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <QDialog>
#include "core/logbook/Qso.h"

class QCheckBox;
class QComboBox;
class QDateTimeEdit;
class QDoubleSpinBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

/// Full QSO entry / edit dialog with tabbed layout.
///
/// Used for both new QSOs (id == -1, opened from the "Full Entry…" button in
/// QsoQuickEntryPanel) and editing existing records (id >= 0, opened from a
/// double-click on the log table). The caller distinguishes the two cases via
/// the Qso::id field and routes the result to insertQso() or updateQso().
class QsoFullEntryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QsoFullEntryDialog(const Qso &qso, QWidget *parent = nullptr);

    Qso qso() const;

private slots:
    void onModeChanged();

private:
    void buildContactTab(class QTabWidget *tabs);
    void buildActivityTab(class QTabWidget *tabs);
    void buildPropagationTab(class QTabWidget *tabs);
    void buildContestTab(class QTabWidget *tabs);
    void buildSatelliteTab(class QTabWidget *tabs);
    void buildNotesTab(class QTabWidget *tabs);

    void loadQso(const Qso &qso);

    // ---- Contact tab ----
    QDateTimeEdit   *m_datetimeOn      = nullptr;
    QCheckBox       *m_datetimeOffEn   = nullptr;
    QDateTimeEdit   *m_datetimeOff     = nullptr;
    QLineEdit       *m_callsign        = nullptr;
    QLineEdit       *m_name            = nullptr;
    QLineEdit       *m_contactedOp     = nullptr;
    QLineEdit       *m_qth             = nullptr;
    QLineEdit       *m_gridsquare      = nullptr;
    QLineEdit       *m_country         = nullptr;
    QLineEdit       *m_state           = nullptr;
    QLineEdit       *m_county          = nullptr;
    QLineEdit       *m_cont            = nullptr;
    QSpinBox        *m_dxcc            = nullptr;
    QSpinBox        *m_cqZone          = nullptr;
    QSpinBox        *m_ituZone         = nullptr;
    QLineEdit       *m_pfx             = nullptr;
    QLineEdit       *m_arrlSect        = nullptr;
    QSpinBox        *m_age             = nullptr;
    QLineEdit       *m_eqCall          = nullptr;
    QLineEdit       *m_stationCallsign = nullptr;
    QDoubleSpinBox  *m_txPwr           = nullptr;
    QLineEdit       *m_antenna         = nullptr;
    QLineEdit       *m_rig             = nullptr;
    QComboBox       *m_band            = nullptr;
    QDoubleSpinBox  *m_freq            = nullptr;
    QLineEdit       *m_bandRx          = nullptr;
    QDoubleSpinBox  *m_freqRx          = nullptr;
    QComboBox       *m_mode            = nullptr;
    QComboBox       *m_submode         = nullptr;
    QLineEdit       *m_rstSent         = nullptr;
    QLineEdit       *m_rstRcvd         = nullptr;
    QLineEdit       *m_comment         = nullptr;
    // QSL status (embedded in Contact tab)
    QComboBox       *m_lotwSent        = nullptr;
    QComboBox       *m_lotwRcvd        = nullptr;
    QComboBox       *m_eqslSent        = nullptr;
    QComboBox       *m_eqslRcvd        = nullptr;
    QComboBox       *m_qrzSent         = nullptr;
    QComboBox       *m_qrzRcvd         = nullptr;
    QComboBox       *m_qslSent         = nullptr;
    QComboBox       *m_qslRcvd         = nullptr;

    // ---- Activity tab ----
    QLineEdit       *m_sig             = nullptr;
    QLineEdit       *m_sigInfo         = nullptr;
    QLineEdit       *m_mySig           = nullptr;
    QLineEdit       *m_mySigInfo       = nullptr;

    // ---- Propagation tab ----
    QLineEdit       *m_propMode        = nullptr;
    QDoubleSpinBox  *m_sfi             = nullptr;
    QDoubleSpinBox  *m_aIndex          = nullptr;
    QDoubleSpinBox  *m_kIndex          = nullptr;
    QSpinBox        *m_sunspots        = nullptr;

    // ---- Contest tab ----
    QLineEdit       *m_contestId       = nullptr;
    QSpinBox        *m_srx             = nullptr;
    QLineEdit       *m_srxString       = nullptr;
    QSpinBox        *m_stx             = nullptr;
    QLineEdit       *m_stxString       = nullptr;
    QLineEdit       *m_precedence      = nullptr;
    QLineEdit       *m_contestClass    = nullptr;
    QLineEdit       *m_arrlCheck       = nullptr;

    // ---- Satellite tab ----
    QLineEdit       *m_satName         = nullptr;
    QLineEdit       *m_satMode         = nullptr;
    QDoubleSpinBox  *m_maxBursts       = nullptr;
    QSpinBox        *m_nrBursts        = nullptr;
    QSpinBox        *m_nrPings         = nullptr;
    QLineEdit       *m_msShower        = nullptr;

    // ---- Notes tab ----
    QPlainTextEdit  *m_notes           = nullptr;
    QLineEdit       *m_qsoComplete     = nullptr;

    Qso m_original;
};
