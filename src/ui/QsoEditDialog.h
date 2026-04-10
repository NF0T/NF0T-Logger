#pragma once

#include <QDialog>
#include "core/logbook/Qso.h"

class QCheckBox;
class QComboBox;
class QDateTimeEdit;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;

/// Modal dialog for editing an existing QSO record.
/// Call qso() after exec() == QDialog::Accepted to retrieve the modified Qso.
class QsoEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QsoEditDialog(const Qso &qso, QWidget *parent = nullptr);

    Qso qso() const;

private:
    void buildUi();
    void loadQso(const Qso &qso);

    // Core
    QDateTimeEdit   *m_datetimeOn  = nullptr;
    QLineEdit       *m_callsign    = nullptr;
    QComboBox       *m_band        = nullptr;
    QDoubleSpinBox  *m_freq        = nullptr;
    QComboBox       *m_mode        = nullptr;
    QComboBox       *m_submode     = nullptr;
    QLineEdit       *m_rstSent     = nullptr;
    QLineEdit       *m_rstRcvd     = nullptr;

    // Contact info
    QLineEdit       *m_name        = nullptr;
    QLineEdit       *m_qth         = nullptr;
    QLineEdit       *m_grid        = nullptr;
    QLineEdit       *m_country     = nullptr;
    QLineEdit       *m_state       = nullptr;

    // Notes
    QLineEdit       *m_comment     = nullptr;
    QLineEdit       *m_notes       = nullptr;

    // QSL
    QComboBox       *m_lotwSent    = nullptr;
    QComboBox       *m_lotwRcvd    = nullptr;
    QComboBox       *m_eqslSent    = nullptr;
    QComboBox       *m_eqslRcvd    = nullptr;

    // My station
    QLineEdit       *m_stationCall = nullptr;
    QDoubleSpinBox  *m_txPwr       = nullptr;

    // Activity
    QLineEdit       *m_sig         = nullptr;
    QLineEdit       *m_sigInfo     = nullptr;

    Qso m_original;
};
