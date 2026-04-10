#include "DisplayPage.h"
#include "app/settings/Settings.h"

#include <QButtonGroup>
#include <QFormLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>

DisplayPage::DisplayPage(QWidget *parent)
    : SettingsPage(parent)
{
    m_imperial = new QRadioButton(tr("Imperial (miles)"), this);
    m_metric   = new QRadioButton(tr("Metric (km)"),     this);

    auto *group = new QButtonGroup(this);
    group->addButton(m_imperial);
    group->addButton(m_metric);

    auto *unitsBox = new QGroupBox(tr("Distance units"), this);
    auto *boxLayout = new QVBoxLayout(unitsBox);
    boxLayout->addWidget(m_imperial);
    boxLayout->addWidget(m_metric);

    auto *root = new QVBoxLayout(this);
    root->addWidget(unitsBox);
    root->addStretch();
}

void DisplayPage::load()
{
    if (Settings::instance().useMetricUnits())
        m_metric->setChecked(true);
    else
        m_imperial->setChecked(true);
}

void DisplayPage::apply()
{
    Settings::instance().setUseMetricUnits(m_metric->isChecked());
}
