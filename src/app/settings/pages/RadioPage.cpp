#include "RadioPage.h"
#include "app/settings/Settings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

RadioPage::RadioPage(QWidget *parent)
    : SettingsPage(parent)
{
    auto *tabs = new QTabWidget(this);

    // -----------------------------------------------------------------------
    // Hamlib tab
    // -----------------------------------------------------------------------
    auto *hamlibWidget = new QWidget;
    m_hamlibEnabled = new QCheckBox(tr("Enable Hamlib radio control"), hamlibWidget);

    m_rigModel = new QSpinBox(hamlibWidget);
    m_rigModel->setRange(1, 9999);
    m_rigModel->setToolTip(tr("Hamlib rig model number (see rigctl --list)"));

    m_connType = new QComboBox(hamlibWidget);
    m_connType->addItem(tr("Serial port"), "serial");
    m_connType->addItem(tr("Network (TCP)"), "network");
    connect(m_connType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RadioPage::onHamlibConnTypeChanged);

    // Serial group
    m_serialGroup  = new QGroupBox(tr("Serial port"), hamlibWidget);
    m_serialDevice = new QLineEdit(m_serialGroup);
    m_serialDevice->setPlaceholderText("/dev/ttyUSB0");

    m_baudRate = new QComboBox(m_serialGroup);
    for (int b : {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200})
        m_baudRate->addItem(QString::number(b), b);

    m_dataBits = new QComboBox(m_serialGroup);
    for (int d : {7, 8}) m_dataBits->addItem(QString::number(d), d);

    m_stopBits = new QComboBox(m_serialGroup);
    for (int s : {1, 2}) m_stopBits->addItem(QString::number(s), s);

    m_parity = new QComboBox(m_serialGroup);
    m_parity->addItems({tr("None"), tr("Even"), tr("Odd")});

    m_flowControl = new QComboBox(m_serialGroup);
    m_flowControl->addItems({tr("None"), tr("Hardware"), tr("Software")});

    auto *serialForm = new QFormLayout(m_serialGroup);
    serialForm->addRow(tr("Device:"),       m_serialDevice);
    serialForm->addRow(tr("Baud rate:"),    m_baudRate);
    serialForm->addRow(tr("Data bits:"),    m_dataBits);
    serialForm->addRow(tr("Stop bits:"),    m_stopBits);
    serialForm->addRow(tr("Parity:"),       m_parity);
    serialForm->addRow(tr("Flow control:"), m_flowControl);

    // Network group
    m_networkGroup = new QGroupBox(tr("Network"), hamlibWidget);
    m_netHost      = new QLineEdit(m_networkGroup);
    m_netHost->setPlaceholderText("localhost");
    m_netPort      = new QSpinBox(m_networkGroup);
    m_netPort->setRange(1, 65535);
    m_netPort->setValue(4532);

    auto *netForm = new QFormLayout(m_networkGroup);
    netForm->addRow(tr("Host:"), m_netHost);
    netForm->addRow(tr("Port:"), m_netPort);

    auto *hamlibForm = new QFormLayout;
    hamlibForm->addRow(m_hamlibEnabled);
    hamlibForm->addRow(tr("Rig model:"),        m_rigModel);
    hamlibForm->addRow(tr("Connection type:"),   m_connType);

    auto *hamlibLayout = new QVBoxLayout(hamlibWidget);
    hamlibLayout->addLayout(hamlibForm);
    hamlibLayout->addWidget(m_serialGroup);
    hamlibLayout->addWidget(m_networkGroup);
    hamlibLayout->addStretch();

    // -----------------------------------------------------------------------
    // TCI tab
    // -----------------------------------------------------------------------
    auto *tciWidget = new QWidget;
    m_tciEnabled = new QCheckBox(tr("Enable TCI radio control"), tciWidget);
    m_tciHost    = new QLineEdit(tciWidget);
    m_tciHost->setPlaceholderText("localhost");
    m_tciPort    = new QSpinBox(tciWidget);
    m_tciPort->setRange(1, 65535);
    m_tciPort->setValue(50001);

    auto *tciForm = new QFormLayout;
    tciForm->addRow(m_tciEnabled);
    tciForm->addRow(tr("Host:"), m_tciHost);
    tciForm->addRow(tr("Port:"), m_tciPort);

    auto *tciLayout = new QVBoxLayout(tciWidget);
    tciLayout->addLayout(tciForm);
    tciLayout->addStretch();

    tabs->addTab(hamlibWidget, tr("Hamlib"));
    tabs->addTab(tciWidget,    tr("TCI"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(tabs);
}

void RadioPage::onHamlibConnTypeChanged(int index)
{
    const QString type = m_connType->itemData(index).toString();
    m_serialGroup->setEnabled(type == "serial");
    m_networkGroup->setEnabled(type == "network");
}

void RadioPage::load()
{
    const Settings &s = Settings::instance();

    m_hamlibEnabled->setChecked(s.hamlibEnabled());
    m_rigModel->setValue(s.hamlibRigModel());

    const int connIdx = m_connType->findData(s.hamlibConnectionType());
    m_connType->setCurrentIndex(connIdx >= 0 ? connIdx : 0);

    m_serialDevice->setText(s.hamlibSerialDevice());

    const int baudIdx = m_baudRate->findData(s.hamlibBaudRate());
    m_baudRate->setCurrentIndex(baudIdx >= 0 ? baudIdx : 3);  // default 9600

    const int dataIdx = m_dataBits->findData(s.hamlibDataBits());
    m_dataBits->setCurrentIndex(dataIdx >= 0 ? dataIdx : 1);  // default 8

    const int stopIdx = m_stopBits->findData(s.hamlibStopBits());
    m_stopBits->setCurrentIndex(stopIdx >= 0 ? stopIdx : 0);  // default 1

    m_parity->setCurrentText(s.hamlibParity());
    m_flowControl->setCurrentText(s.hamlibFlowControl());
    m_netHost->setText(s.hamlibNetworkHost());
    m_netPort->setValue(s.hamlibNetworkPort());

    m_tciEnabled->setChecked(s.tciEnabled());
    m_tciHost->setText(s.tciHost());
    m_tciPort->setValue(s.tciPort());

    onHamlibConnTypeChanged(m_connType->currentIndex());
}

void RadioPage::apply()
{
    Settings &s = Settings::instance();

    s.setHamlibEnabled(m_hamlibEnabled->isChecked());
    s.setHamlibRigModel(m_rigModel->value());
    s.setHamlibConnectionType(m_connType->currentData().toString());
    s.setHamlibSerialDevice(m_serialDevice->text().trimmed());
    s.setHamlibBaudRate(m_baudRate->currentData().toInt());
    s.setHamlibDataBits(m_dataBits->currentData().toInt());
    s.setHamlibStopBits(m_stopBits->currentData().toInt());
    s.setHamlibParity(m_parity->currentText());
    s.setHamlibFlowControl(m_flowControl->currentText());
    s.setHamlibNetworkHost(m_netHost->text().trimmed());
    s.setHamlibNetworkPort(m_netPort->value());

    s.setTciEnabled(m_tciEnabled->isChecked());
    s.setTciHost(m_tciHost->text().trimmed());
    s.setTciPort(m_tciPort->value());
}
