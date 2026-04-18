// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include "SettingsPage.h"

#include <QList>
#include <QString>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QSpinBox;

class RadioPage : public SettingsPage
{
    Q_OBJECT

public:
    explicit RadioPage(QWidget *parent = nullptr);

    QString pageTitle() const override { return tr("Radio"); }
    void load()  override;
    void apply() override;

    // Public so the Hamlib C callback (file scope) can access it
    struct RigEntry {
        int     model   = 0;
        QString mfg;
        QString name;
        int     status  = 0;
        int     maxBaud = 0;
    };

private slots:
    void onHamlibConnTypeChanged(int index);
    void onRigFilterChanged(const QString &text);
    void onShowAllStatusChanged(Qt::CheckState state);
    void onRigSelected(int index);

private:
    void populateRigCombo();   // rebuild combo from m_allRigs + current filter
    void loadRigList();        // call once in constructor via rig_list_foreach

    QList<RigEntry> m_allRigs; // sorted by mfg then name

    // Hamlib
    QCheckBox  *m_hamlibEnabled    = nullptr;
    QLineEdit  *m_rigFilter        = nullptr;
    QComboBox  *m_rigCombo         = nullptr;
    QCheckBox  *m_showAllStatus    = nullptr;
    QLabel     *m_rigStatusLabel   = nullptr;   // shows status of selected rig
    QComboBox  *m_connType         = nullptr;

    QGroupBox  *m_serialGroup      = nullptr;
    QLineEdit  *m_serialDevice     = nullptr;
    QComboBox  *m_baudRate         = nullptr;
    QComboBox  *m_dataBits         = nullptr;
    QComboBox  *m_stopBits         = nullptr;
    QComboBox  *m_parity           = nullptr;
    QComboBox  *m_flowControl      = nullptr;

    QGroupBox  *m_networkGroup     = nullptr;
    QLineEdit  *m_netHost          = nullptr;
    QSpinBox   *m_netPort          = nullptr;

    // TCI
    QCheckBox  *m_tciEnabled       = nullptr;
    QLineEdit  *m_tciHost          = nullptr;
    QSpinBox   *m_tciPort          = nullptr;
};
