// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "WsjtxPage.h"
#include "app/settings/Settings.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QNetworkInterface>
#include <QSpinBox>
#include <QVBoxLayout>

WsjtxPage::WsjtxPage(QWidget *parent)
    : SettingsPage(parent)
{
    m_enabled = new QCheckBox(tr("Enable WSJT-X UDP listener"), this);

    m_port = new QSpinBox(this);
    m_port->setRange(1024, 65535);
    m_port->setValue(2237);

    m_multicastGroup = new QLineEdit(this);
    m_multicastGroup->setPlaceholderText(tr("224.0.0.1 (multicast) or leave blank for unicast"));

    m_autoLog = new QCheckBox(tr("Automatically log QSOs received from WSJT-X"), this);

    auto *note = new QLabel(
        tr("<i>Enter a multicast address (e.g. 224.0.0.1) to receive alongside "
           "GridTracker2 or JTAlert simultaneously. Enter a unicast address "
           "(e.g. 127.0.0.1) or leave blank to bind any interface for unicast only.</i>"),
        this);
    note->setWordWrap(true);

    // Multicast interface selector — only visible when address is multicast
    m_ifaceList = new QListWidget(this);
    m_ifaceList->setSelectionMode(QAbstractItemView::NoSelection);
    m_ifaceList->setMaximumHeight(120);
    populateIfaceList();

    m_ifaceBox = new QGroupBox(tr("Multicast interfaces"), this);
    auto *ifaceBoxLayout = new QVBoxLayout(m_ifaceBox);
    ifaceBoxLayout->setContentsMargins(6, 6, 6, 6);
    ifaceBoxLayout->addWidget(new QLabel(
        tr("<i>Check each interface WSJT-X is bound to. "
           "Leave all unchecked to join on all multicast-capable interfaces automatically.</i>"),
        this));
    ifaceBoxLayout->addWidget(m_ifaceList);

    auto *form = new QFormLayout;
    form->addRow(m_enabled);
    form->addRow(tr("UDP port:"),    m_port);
    form->addRow(tr("UDP address:"), m_multicastGroup);
    form->addRow(m_autoLog);
    form->addRow(note);

    auto *root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addWidget(m_ifaceBox);
    root->addStretch();

    connect(m_multicastGroup, &QLineEdit::textChanged,
            this, &WsjtxPage::onAddressChanged);
}

void WsjtxPage::populateIfaceList()
{
    m_ifaceList->clear();
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : ifaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp)) continue;

        // Build a label: "eth0 — 192.168.1.5"  (first IPv4 address, if any)
        QString addr;
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                addr = entry.ip().toString();
                break;
            }
        }
        const QString label = addr.isEmpty()
            ? iface.name()
            : QStringLiteral("%1 — %2").arg(iface.name(), addr);

        auto *item = new QListWidgetItem(label, m_ifaceList);
        item->setData(Qt::UserRole, iface.name());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
}

void WsjtxPage::onAddressChanged(const QString &text)
{
    const QHostAddress addr(text.trimmed());
    m_ifaceBox->setVisible(addr.isMulticast());
}

void WsjtxPage::load()
{
    const Settings &s = Settings::instance();
    m_enabled->setChecked(s.wsjtxEnabled());
    m_port->setValue(s.wsjtxPort());
    m_multicastGroup->setText(s.wsjtxUdpAddress());
    m_autoLog->setChecked(s.wsjtxAutoLog());

    // Restore checked interfaces
    const QStringList saved = s.wsjtxMulticastIfaces();
    for (int i = 0; i < m_ifaceList->count(); ++i) {
        QListWidgetItem *item = m_ifaceList->item(i);
        const bool checked = saved.contains(item->data(Qt::UserRole).toString());
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }

    // Show/hide iface box based on current address
    onAddressChanged(s.wsjtxUdpAddress());
}

void WsjtxPage::apply()
{
    Settings &s = Settings::instance();
    s.setWsjtxEnabled(m_enabled->isChecked());
    s.setWsjtxPort(static_cast<quint16>(m_port->value()));
    s.setWsjtxUdpAddress(m_multicastGroup->text().trimmed());
    s.setWsjtxAutoLog(m_autoLog->isChecked());

    QStringList selected;
    for (int i = 0; i < m_ifaceList->count(); ++i) {
        const QListWidgetItem *item = m_ifaceList->item(i);
        if (item->checkState() == Qt::Checked)
            selected << item->data(Qt::UserRole).toString();
    }
    s.setWsjtxMulticastIfaces(selected);
}
