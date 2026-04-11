#include "WsjtxPage.h"
#include "app/settings/Settings.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

WsjtxPage::WsjtxPage(QWidget *parent)
    : SettingsPage(parent)
{
    m_enabled        = new QCheckBox(tr("Enable WSJT-X UDP listener"), this);
    m_port           = new QSpinBox(this);
    m_port->setRange(1024, 65535);
    m_port->setValue(2237);
    m_multicastGroup = new QLineEdit(this);
    m_multicastGroup->setPlaceholderText(tr("224.0.0.1 (multicast) or leave blank for unicast"));
    m_autoLog        = new QCheckBox(tr("Automatically log QSOs received from WSJT-X"), this);

    auto *note = new QLabel(
        tr("<i>Enter a multicast address (e.g. 224.0.0.1) to receive alongside "
           "GridTracker2 or JTAlert simultaneously. Enter a unicast address "
           "(e.g. 127.0.0.1) or leave blank to bind any interface for unicast only.</i>"),
        this);
    note->setWordWrap(true);

    auto *form = new QFormLayout;
    form->addRow(m_enabled);
    form->addRow(tr("UDP port:"),        m_port);
    form->addRow(tr("UDP address:"),     m_multicastGroup);
    form->addRow(m_autoLog);
    form->addRow(note);

    auto *root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addStretch();
}

void WsjtxPage::load()
{
    const Settings &s = Settings::instance();
    m_enabled->setChecked(s.wsjtxEnabled());
    m_port->setValue(s.wsjtxPort());
    m_multicastGroup->setText(s.wsjtxUdpAddress());
    m_autoLog->setChecked(s.wsjtxAutoLog());
}

void WsjtxPage::apply()
{
    Settings &s = Settings::instance();
    s.setWsjtxEnabled(m_enabled->isChecked());
    s.setWsjtxPort(static_cast<quint16>(m_port->value()));
    s.setWsjtxUdpAddress(m_multicastGroup->text().trimmed());
    s.setWsjtxAutoLog(m_autoLog->isChecked());
}
