#pragma once

#include "SettingsPage.h"

class QCheckBox;
class QComboBox;
class QGroupBox;
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

private slots:
    void onHamlibConnTypeChanged(int index);

private:
    // Hamlib
    QCheckBox  *m_hamlibEnabled    = nullptr;
    QSpinBox   *m_rigModel         = nullptr;
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
