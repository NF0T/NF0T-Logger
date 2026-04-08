#include "EquipmentPage.h"
#include "app/settings/Settings.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>

EquipmentPage::EquipmentPage(QWidget *parent)
    : SettingsPage(parent)
{
    m_rig     = new QLineEdit(this);
    m_rig->setPlaceholderText(tr("e.g. Icom IC-7300"));

    m_antenna = new QLineEdit(this);
    m_antenna->setPlaceholderText(tr("e.g. Dipole @ 30 ft"));

    m_txPwr   = new QDoubleSpinBox(this);
    m_txPwr->setRange(0.0, 2000.0);
    m_txPwr->setDecimals(1);
    m_txPwr->setSuffix(tr(" W"));

    auto *form = new QFormLayout;
    form->addRow(tr("Default rig:"),     m_rig);
    form->addRow(tr("Default antenna:"), m_antenna);
    form->addRow(tr("Default TX power:"),m_txPwr);

    auto *root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addStretch();
}

void EquipmentPage::load()
{
    const Settings &s = Settings::instance();
    m_rig->setText(s.equipmentRig());
    m_antenna->setText(s.equipmentAntenna());
    m_txPwr->setValue(s.equipmentTxPwr());
}

void EquipmentPage::apply()
{
    Settings &s = Settings::instance();
    s.setEquipmentRig(m_rig->text().trimmed());
    s.setEquipmentAntenna(m_antenna->text().trimmed());
    s.setEquipmentTxPwr(m_txPwr->value());
}
