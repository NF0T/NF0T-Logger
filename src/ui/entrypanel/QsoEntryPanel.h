#pragma once

#include <QWidget>
#include "core/logbook/Qso.h"

class QComboBox;
class QDateTimeEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;

/// Compact QSO entry panel embedded at the bottom of the main window.
///
/// Workflow:
///   1. Type callsign (auto-uppercase).
///   2. Adjust band / freq / mode if needed (sticky from last QSO).
///   3. Enter RST, name, and any other fields.
///   4. Press Log (or Enter from RST Rcvd field) to save.
///
/// The panel emits qsoReady() — MainWindow inserts the QSO into the
/// database and appends it to the table model.
///
/// Radio integration (Hamlib / TCI) will update freq/mode via
/// setRadioFreq() / setRadioMode() when those backends are implemented.
class QsoEntryPanel : public QWidget
{
    Q_OBJECT

public:
    explicit QsoEntryPanel(QWidget *parent = nullptr);

    /// Called by radio backends when the rig's frequency changes.
    void setRadioFreq(double freqMhz);

    /// Called by radio backends when the rig's mode changes.
    void setRadioMode(const QString &mode, const QString &submode = {});

public slots:
    /// Reset to a blank contact, preserving band/mode/power from last entry.
    void clearForm();

signals:
    /// Emitted when the user clicks "Log". The Qso is fully populated
    /// with station defaults from Settings. Receiver should insert into DB.
    void qsoReady(const Qso &qso);

private slots:
    void onModeChanged(int index);
    void onFreqChanged(double freqMhz);
    void onBandChanged(int index);
    void onLogClicked();
    void onNowClicked();

private:
    Qso buildQso() const;
    bool validate() const;
    void applyStationDefaults(Qso &qso) const;
    void updateRstDefaults();
    void setFreqSilently(double freqMhz);
    void setBandSilently(const QString &band);

    // Row 1 — time, callsign, band/freq/mode
    QDateTimeEdit  *m_dateTime   = nullptr;
    QPushButton    *m_nowBtn     = nullptr;
    QLineEdit      *m_callsign   = nullptr;
    QComboBox      *m_band       = nullptr;
    QDoubleSpinBox *m_freq       = nullptr;
    QComboBox      *m_mode       = nullptr;
    QComboBox      *m_submode    = nullptr;

    // Row 2 — exchange / operator info
    QLineEdit      *m_rstSent    = nullptr;
    QLineEdit      *m_rstRcvd   = nullptr;
    QLineEdit      *m_name       = nullptr;
    QLineEdit      *m_grid       = nullptr;
    QDoubleSpinBox *m_txPwr      = nullptr;

    // Row 3 — activity refs, comment, actions
    QLineEdit      *m_sig        = nullptr;
    QLineEdit      *m_sigInfo    = nullptr;
    QLineEdit      *m_comment    = nullptr;
    QPushButton    *m_logBtn     = nullptr;
    QPushButton    *m_clearBtn   = nullptr;

    // Status label shown in entry panel
    QLabel         *m_statusLabel = nullptr;

    // Guard against recursive signal firing during programmatic changes
    bool m_suppressBandFreqSignals = false;
};
