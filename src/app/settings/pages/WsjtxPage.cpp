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

    m_autoLog = new QCheckBox(tr("Automatically log QSOs received from WSJT-X"), this);

    // Multicast toggle
    m_useMulticast = new QCheckBox(tr("Use multicast (share port with GridTracker2 / JTAlert)"), this);

    // Multicast group box — visible only when multicast is enabled
    m_multicastBox = new QGroupBox(tr("Multicast"), this);
    m_multicastBox->setVisible(false);

    m_multicastGroup = new QLineEdit(m_multicastBox);
    m_multicastGroup->setPlaceholderText(tr("224.0.0.1"));

    auto *addrNote = new QLabel(
        tr("<i>Multicast group address shared with WSJT-X and other listeners "
           "(typically 224.0.0.1).</i>"),
        m_multicastBox);
    addrNote->setWordWrap(true);

    m_ifaceList = new QListWidget(m_multicastBox);
    m_ifaceList->setSelectionMode(QAbstractItemView::NoSelection);
    m_ifaceList->setMaximumHeight(120);
    populateIfaceList();

    m_ifaceBox = new QGroupBox(tr("Multicast interfaces"), m_multicastBox);
    auto *ifaceBoxLayout = new QVBoxLayout(m_ifaceBox);
    ifaceBoxLayout->setContentsMargins(6, 6, 6, 6);
    ifaceBoxLayout->addWidget(new QLabel(
        tr("<i>Check each interface WSJT-X is bound to. "
           "Leave all unchecked to join on all multicast-capable interfaces automatically.</i>"),
        m_ifaceBox));
    ifaceBoxLayout->addWidget(m_ifaceList);

    auto *mcForm = new QFormLayout;
    mcForm->addRow(tr("Group address:"), m_multicastGroup);
    mcForm->addRow(addrNote);
    mcForm->addRow(m_ifaceBox);

    auto *mcBoxLayout = new QVBoxLayout(m_multicastBox);
    mcBoxLayout->addLayout(mcForm);

    auto *form = new QFormLayout;
    form->addRow(m_enabled);
    form->addRow(tr("UDP port:"), m_port);
    form->addRow(m_autoLog);
    form->addRow(m_useMulticast);

    auto *root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addWidget(m_multicastBox);
    root->addStretch();

    connect(m_useMulticast, &QCheckBox::toggled,
            this, &WsjtxPage::onMulticastToggled);
    connect(m_multicastGroup, &QLineEdit::textChanged,
            this, &WsjtxPage::onAddressChanged);
}

void WsjtxPage::populateIfaceList()
{
    m_ifaceList->clear();
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : ifaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp)) continue;

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

void WsjtxPage::onMulticastToggled(bool checked)
{
    m_multicastBox->setVisible(checked);
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
    m_autoLog->setChecked(s.wsjtxAutoLog());

    const QString udpAddr = s.wsjtxUdpAddress();
    const bool isMulticast = QHostAddress(udpAddr).isMulticast();

    m_useMulticast->setChecked(isMulticast);
    m_multicastBox->setVisible(isMulticast);

    if (isMulticast)
        m_multicastGroup->setText(udpAddr);
    else
        m_multicastGroup->setText(QStringLiteral("224.0.0.1"));

    // Restore checked interfaces
    const QStringList saved = s.wsjtxMulticastIfaces();
    for (int i = 0; i < m_ifaceList->count(); ++i) {
        QListWidgetItem *item = m_ifaceList->item(i);
        item->setCheckState(
            saved.contains(item->data(Qt::UserRole).toString()) ? Qt::Checked : Qt::Unchecked);
    }

    onAddressChanged(m_multicastGroup->text());
}

void WsjtxPage::apply()
{
    Settings &s = Settings::instance();
    s.setWsjtxEnabled(m_enabled->isChecked());
    s.setWsjtxPort(static_cast<quint16>(m_port->value()));
    s.setWsjtxAutoLog(m_autoLog->isChecked());

    // Save multicast address when enabled, empty string for unicast
    s.setWsjtxUdpAddress(m_useMulticast->isChecked()
        ? m_multicastGroup->text().trimmed()
        : QString());

    QStringList selected;
    if (m_useMulticast->isChecked()) {
        for (int i = 0; i < m_ifaceList->count(); ++i) {
            const QListWidgetItem *item = m_ifaceList->item(i);
            if (item->checkState() == Qt::Checked)
                selected << item->data(Qt::UserRole).toString();
        }
    }
    s.setWsjtxMulticastIfaces(selected);
}
