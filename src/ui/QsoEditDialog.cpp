// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "QsoEditDialog.h"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QTimeZone>
#include <QVBoxLayout>

#include "core/BandPlan.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

QsoEditDialog::QsoEditDialog(const Qso &qso, QWidget *parent)
    : QDialog(parent)
    , m_original(qso)
{
    setWindowTitle(tr("Edit QSO — %1").arg(qso.callsign));
    setMinimumWidth(540);
    buildUi();
    loadQso(qso);
}

// ---------------------------------------------------------------------------
// Build UI
// ---------------------------------------------------------------------------

static QComboBox *qslStatusCombo(QWidget *parent)
{
    auto *cb = new QComboBox(parent);
    cb->addItem("N", QChar('N'));
    cb->addItem("Y", QChar('Y'));
    cb->addItem("R", QChar('R'));
    cb->addItem("Q", QChar('Q'));
    cb->addItem("I", QChar('I'));
    return cb;
}

static void setQslStatus(QComboBox *cb, QChar status)
{
    const int idx = cb->findData(status);
    cb->setCurrentIndex(idx >= 0 ? idx : 0);
}

static QChar getQslStatus(const QComboBox *cb)
{
    return cb->currentData().value<QChar>();
}

void QsoEditDialog::buildUi()
{
    auto *tabs = new QTabWidget(this);

    // -----------------------------------------------------------------------
    // Tab 1 — Core
    // -----------------------------------------------------------------------
    auto *coreWidget = new QWidget;

    m_datetimeOn = new QDateTimeEdit(coreWidget);
    m_datetimeOn->setTimeZone(QTimeZone::utc());
    m_datetimeOn->setDisplayFormat("yyyy-MM-dd HH:mm:ss");

    m_callsign = new QLineEdit(coreWidget);
    m_callsign->setMaxLength(20);

    m_band = new QComboBox(coreWidget);
    m_band->addItem(tr("—"));
    m_band->addItems(BandPlan::bandNames());

    m_freq = new QDoubleSpinBox(coreWidget);
    m_freq->setRange(0.0, 999999.0);
    m_freq->setDecimals(6);
    m_freq->setSuffix(tr(" MHz"));

    m_mode = new QComboBox(coreWidget);
    m_mode->addItems(BandPlan::modeNames());

    m_submode = new QComboBox(coreWidget);
    connect(m_mode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        const QStringList subs = BandPlan::submodesForMode(m_mode->currentText());
        m_submode->clear();
        m_submode->setEnabled(!subs.isEmpty());
        if (!subs.isEmpty())
            m_submode->addItems(subs);
    });

    m_rstSent = new QLineEdit(coreWidget);
    m_rstSent->setMaxLength(6);
    m_rstRcvd = new QLineEdit(coreWidget);
    m_rstRcvd->setMaxLength(6);

    auto *rstRow = new QHBoxLayout;
    rstRow->addWidget(m_rstSent);
    rstRow->addWidget(new QLabel(tr("/"), coreWidget));
    rstRow->addWidget(m_rstRcvd);
    rstRow->addStretch();

    auto *bandFreqRow = new QHBoxLayout;
    bandFreqRow->addWidget(m_band);
    bandFreqRow->addWidget(m_freq, 1);

    auto *modeRow = new QHBoxLayout;
    modeRow->addWidget(m_mode);
    modeRow->addWidget(m_submode);
    modeRow->addStretch();

    auto *coreForm = new QFormLayout(coreWidget);
    coreForm->addRow(tr("Date/Time (UTC):"), m_datetimeOn);
    coreForm->addRow(tr("Callsign:"),        m_callsign);
    coreForm->addRow(tr("Band / Freq:"),     bandFreqRow);
    coreForm->addRow(tr("Mode:"),            modeRow);
    coreForm->addRow(tr("RST Sent / Rcvd:"), rstRow);

    // -----------------------------------------------------------------------
    // Tab 2 — Contact info
    // -----------------------------------------------------------------------
    auto *contactWidget = new QWidget;

    m_name    = new QLineEdit(contactWidget);
    m_qth     = new QLineEdit(contactWidget);
    m_grid    = new QLineEdit(contactWidget);
    m_grid->setMaxLength(10);
    m_country = new QLineEdit(contactWidget);
    m_state   = new QLineEdit(contactWidget);
    m_comment = new QLineEdit(contactWidget);
    m_notes   = new QLineEdit(contactWidget);
    m_txPwr   = new QDoubleSpinBox(contactWidget);
    m_txPwr->setRange(0.0, 2000.0);
    m_txPwr->setDecimals(1);
    m_txPwr->setSuffix(tr(" W"));
    m_stationCall = new QLineEdit(contactWidget);
    m_sig     = new QLineEdit(contactWidget);
    m_sig->setMaxLength(30);
    m_sigInfo = new QLineEdit(contactWidget);

    auto *contactForm = new QFormLayout(contactWidget);
    contactForm->addRow(tr("Name:"),            m_name);
    contactForm->addRow(tr("QTH:"),             m_qth);
    contactForm->addRow(tr("Grid:"),            m_grid);
    contactForm->addRow(tr("Country:"),         m_country);
    contactForm->addRow(tr("State:"),           m_state);
    contactForm->addRow(tr("Comment:"),         m_comment);
    contactForm->addRow(tr("Notes:"),           m_notes);
    contactForm->addRow(tr("Station Call:"),    m_stationCall);
    contactForm->addRow(tr("TX Power:"),        m_txPwr);
    contactForm->addRow(tr("Activity (SIG):"),  m_sig);
    contactForm->addRow(tr("Reference:"),       m_sigInfo);

    // -----------------------------------------------------------------------
    // Tab 3 — QSL
    // -----------------------------------------------------------------------
    auto *qslWidget = new QWidget;

    m_lotwSent = qslStatusCombo(qslWidget);
    m_lotwRcvd = qslStatusCombo(qslWidget);
    m_eqslSent = qslStatusCombo(qslWidget);
    m_eqslRcvd = qslStatusCombo(qslWidget);

    auto *lotwRow = new QHBoxLayout;
    lotwRow->addWidget(new QLabel(tr("Sent:"), qslWidget));
    lotwRow->addWidget(m_lotwSent);
    lotwRow->addWidget(new QLabel(tr("Rcvd:"), qslWidget));
    lotwRow->addWidget(m_lotwRcvd);
    lotwRow->addStretch();

    auto *eqslRow = new QHBoxLayout;
    eqslRow->addWidget(new QLabel(tr("Sent:"), qslWidget));
    eqslRow->addWidget(m_eqslSent);
    eqslRow->addWidget(new QLabel(tr("Rcvd:"), qslWidget));
    eqslRow->addWidget(m_eqslRcvd);
    eqslRow->addStretch();

    auto *qslForm = new QFormLayout(qslWidget);
    qslForm->addRow(tr("LoTW:"),  lotwRow);
    qslForm->addRow(tr("eQSL:"),  eqslRow);

    tabs->addTab(coreWidget,    tr("Core"));
    tabs->addTab(contactWidget, tr("Contact"));
    tabs->addTab(qslWidget,     tr("QSL"));

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *root = new QVBoxLayout(this);
    root->addWidget(tabs);
    root->addWidget(buttons);
}

// ---------------------------------------------------------------------------
// Load / save
// ---------------------------------------------------------------------------

void QsoEditDialog::loadQso(const Qso &q)
{
    m_datetimeOn->setDateTime(q.datetimeOn.toUTC());
    m_callsign->setText(q.callsign);

    const int bandIdx = m_band->findText(q.band, Qt::MatchFixedString);
    m_band->setCurrentIndex(bandIdx >= 0 ? bandIdx : 0);
    m_freq->setValue(q.freq);

    const int modeIdx = m_mode->findText(q.mode, Qt::MatchFixedString);
    if (modeIdx >= 0) m_mode->setCurrentIndex(modeIdx);
    if (!q.submode.isEmpty()) {
        const int sIdx = m_submode->findText(q.submode, Qt::MatchFixedString);
        if (sIdx >= 0) m_submode->setCurrentIndex(sIdx);
    }

    m_rstSent->setText(q.rstSent);
    m_rstRcvd->setText(q.rstRcvd);
    m_name->setText(q.name);
    m_qth->setText(q.qth);
    m_grid->setText(q.gridsquare);
    m_country->setText(q.country);
    m_state->setText(q.state);
    m_comment->setText(q.comment);
    m_notes->setText(q.notes);
    m_stationCall->setText(q.stationCallsign);
    m_txPwr->setValue(q.txPwr.value_or(0.0));
    m_sig->setText(q.sig);
    m_sigInfo->setText(q.sigInfo);

    setQslStatus(m_lotwSent, q.lotwQslSent);
    setQslStatus(m_lotwRcvd, q.lotwQslRcvd);
    setQslStatus(m_eqslSent, q.eqslQslSent);
    setQslStatus(m_eqslRcvd, q.eqslQslRcvd);
}

Qso QsoEditDialog::qso() const
{
    Qso q = m_original;  // preserve all fields we don't expose in the UI

    q.datetimeOn     = m_datetimeOn->dateTime().toUTC();
    q.callsign       = m_callsign->text().toUpper().trimmed();
    q.band           = m_band->currentIndex() > 0 ? m_band->currentText() : QString();
    q.freq           = m_freq->value();
    q.mode           = m_mode->currentText();
    q.submode        = m_submode->isEnabled() ? m_submode->currentText() : QString();
    q.rstSent        = m_rstSent->text().trimmed();
    q.rstRcvd        = m_rstRcvd->text().trimmed();
    q.name           = m_name->text().trimmed();
    q.qth            = m_qth->text().trimmed();
    q.gridsquare     = m_grid->text().toUpper().trimmed();
    q.country        = m_country->text().trimmed();
    q.state          = m_state->text().trimmed();
    q.comment        = m_comment->text().trimmed();
    q.notes          = m_notes->text().trimmed();
    q.stationCallsign = m_stationCall->text().toUpper().trimmed();
    const double pwr = m_txPwr->value();
    q.txPwr          = pwr > 0.0 ? std::optional<double>(pwr) : std::nullopt;
    q.sig            = m_sig->text().toUpper().trimmed();
    q.sigInfo        = m_sigInfo->text().toUpper().trimmed();

    q.lotwQslSent    = getQslStatus(m_lotwSent);
    q.lotwQslRcvd    = getQslStatus(m_lotwRcvd);
    q.eqslQslSent    = getQslStatus(m_eqslSent);
    q.eqslQslRcvd    = getQslStatus(m_eqslRcvd);

    return q;
}
