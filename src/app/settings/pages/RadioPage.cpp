#include "RadioPage.h"
#include "app/settings/Settings.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#ifdef HAVE_HAMLIB
#include <hamlib/rig.h>
#endif

// ---------------------------------------------------------------------------
// Rig list helpers
// ---------------------------------------------------------------------------

#ifdef HAVE_HAMLIB
static QString rigStatusLabel(int status)
{
    switch (status) {
    case RIG_STATUS_STABLE:   return QStringLiteral("Stable");
    case RIG_STATUS_BETA:     return QStringLiteral("Beta");
    case RIG_STATUS_ALPHA:    return QStringLiteral("Alpha");
    case RIG_STATUS_BUGGY:    return QStringLiteral("Buggy");
    default:                  return QStringLiteral("Untested");
    }
}

static int collectRig(const struct rig_caps *caps, rig_ptr_t data)
{
    auto *list = static_cast<QList<RadioPage::RigEntry>*>(data);
    RadioPage::RigEntry e;
    e.model   = static_cast<int>(caps->rig_model);
    e.mfg     = QString::fromLatin1(caps->mfg_name   ? caps->mfg_name   : "");
    e.name    = QString::fromLatin1(caps->model_name  ? caps->model_name : "");
    e.status  = static_cast<int>(caps->status);
    e.maxBaud = caps->serial_rate_max;
    list->append(e);
    return 1;   // continue iteration
}
#endif

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

RadioPage::RadioPage(QWidget *parent)
    : SettingsPage(parent)
{
    loadRigList();

    auto *tabs = new QTabWidget(this);

    // -----------------------------------------------------------------------
    // Hamlib tab
    // -----------------------------------------------------------------------
    auto *hamlibWidget = new QWidget;
    m_hamlibEnabled = new QCheckBox(tr("Enable Hamlib radio control"), hamlibWidget);

    // Rig model selector
    m_rigFilter = new QLineEdit(hamlibWidget);
    m_rigFilter->setPlaceholderText(tr("Search manufacturer or model…"));
    m_rigFilter->setClearButtonEnabled(true);

    m_rigCombo = new QComboBox(hamlibWidget);
    m_rigCombo->setMaxVisibleItems(20);
    m_rigCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_rigCombo->setMinimumWidth(300);

    m_showAllStatus = new QCheckBox(tr("Show alpha / untested drivers"), hamlibWidget);

    m_rigStatusLabel = new QLabel(hamlibWidget);
    m_rigStatusLabel->setStyleSheet(QStringLiteral("color: gray; font-style: italic;"));

    connect(m_rigFilter,     &QLineEdit::textChanged,
            this,            &RadioPage::onRigFilterChanged);
    connect(m_showAllStatus, &QCheckBox::checkStateChanged,
            this,            &RadioPage::onShowAllStatusChanged);
    connect(m_rigCombo,      QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,            &RadioPage::onRigSelected);

    populateRigCombo();

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
    hamlibForm->addRow(tr("Rig model:"),      m_rigFilter);
    hamlibForm->addRow(QString(),             m_rigCombo);
    hamlibForm->addRow(QString(),             m_showAllStatus);
    hamlibForm->addRow(QString(),             m_rigStatusLabel);
    hamlibForm->addRow(tr("Connection type:"), m_connType);

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

// ---------------------------------------------------------------------------
// Rig list population
// ---------------------------------------------------------------------------

void RadioPage::loadRigList()
{
#ifdef HAVE_HAMLIB
    rig_load_all_backends();
    rig_list_foreach(collectRig, static_cast<rig_ptr_t>(&m_allRigs));

    std::sort(m_allRigs.begin(), m_allRigs.end(), [](const RigEntry &a, const RigEntry &b) {
        if (a.mfg != b.mfg)   return a.mfg  < b.mfg;
        return a.name < b.name;
    });
#endif
}

void RadioPage::populateRigCombo()
{
    // Block signals while rebuilding so onRigSelected doesn't fire mid-populate
    const QSignalBlocker blocker(m_rigCombo);

    const int previousModel = m_rigCombo->currentData().toInt();

    m_rigCombo->clear();

    if (m_allRigs.isEmpty()) {
        m_rigCombo->addItem(tr("(Hamlib not available)"), 0);
        m_rigCombo->setEnabled(false);
        m_rigFilter->setEnabled(false);
        m_showAllStatus->setEnabled(false);
        return;
    }

    const QString filter  = m_rigFilter->text().trimmed();
    const bool    showAll = m_showAllStatus->isChecked();

    int selectIdx = -1;

    for (const RigEntry &e : m_allRigs) {
        // Status filter: hide alpha and untested unless checkbox is set
#ifdef HAVE_HAMLIB
        if (!showAll && (e.status == RIG_STATUS_ALPHA ||
                         e.status == RIG_STATUS_UNTESTED)) {
            continue;
        }

        // Text filter: match against "Mfg Name" combined string
        if (!filter.isEmpty()) {
            const QString combined = e.mfg + QLatin1Char(' ') + e.name;
            if (!combined.contains(filter, Qt::CaseInsensitive))
                continue;
        }

        const QString label = QStringLiteral("%1 %2 (%3)")
            .arg(e.mfg, e.name, rigStatusLabel(e.status));
        m_rigCombo->addItem(label, e.model);

        if (e.model == previousModel)
            selectIdx = m_rigCombo->count() - 1;
#endif
    }

    if (selectIdx >= 0)
        m_rigCombo->setCurrentIndex(selectIdx);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void RadioPage::onHamlibConnTypeChanged(int index)
{
    const QString type = m_connType->itemData(index).toString();
    m_serialGroup->setEnabled(type == "serial");
    m_networkGroup->setEnabled(type == "network");
}

void RadioPage::onRigFilterChanged(const QString &)
{
    populateRigCombo();
}

void RadioPage::onShowAllStatusChanged(Qt::CheckState)
{
    populateRigCombo();
}

void RadioPage::onRigSelected(int index)
{
    if (index < 0) {
        m_rigStatusLabel->clear();
        return;
    }

    const int model = m_rigCombo->itemData(index).toInt();

#ifdef HAVE_HAMLIB
    // Find the entry to show status and auto-fill baud rate
    for (const RigEntry &e : m_allRigs) {
        if (e.model != model) continue;

        m_rigStatusLabel->setText(
            tr("Model #%1  |  Driver: %2").arg(model).arg(rigStatusLabel(e.status)));

        if (e.maxBaud > 0) {
            // Select the highest baud rate at or below the rig's maximum
            int bestIdx = 0;
            for (int i = 0; i < m_baudRate->count(); ++i) {
                if (m_baudRate->itemData(i).toInt() <= e.maxBaud)
                    bestIdx = i;
            }
            m_baudRate->setCurrentIndex(bestIdx);
        }
        return;
    }
#else
    m_rigStatusLabel->setText(tr("Model #%1").arg(model));
#endif
}

// ---------------------------------------------------------------------------
// Load / apply
// ---------------------------------------------------------------------------

void RadioPage::load()
{
    const Settings &s = Settings::instance();

    m_hamlibEnabled->setChecked(s.hamlibEnabled());

    // Find the saved rig model in the combo; if filtered out, expand the list
    const int savedModel = s.hamlibRigModel();
    {
        const QSignalBlocker blocker(m_rigCombo);
        int idx = -1;
        for (int i = 0; i < m_rigCombo->count(); ++i) {
            if (m_rigCombo->itemData(i).toInt() == savedModel) {
                idx = i;
                break;
            }
        }
        if (idx < 0 && savedModel > 0) {
            // Saved rig was filtered out — expand to show all and retry
            m_showAllStatus->setChecked(true);
            populateRigCombo();
            for (int i = 0; i < m_rigCombo->count(); ++i) {
                if (m_rigCombo->itemData(i).toInt() == savedModel) {
                    idx = i;
                    break;
                }
            }
        }
        if (idx >= 0)
            m_rigCombo->setCurrentIndex(idx);
    }
    // Trigger status label + baud suggestion for the loaded selection
    onRigSelected(m_rigCombo->currentIndex());

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
    s.setHamlibRigModel(m_rigCombo->currentData().toInt());
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
