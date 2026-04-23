// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "QsoFullEntryDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimeZone>
#include <QVBoxLayout>

#include "core/BandPlan.h"

// ---------------------------------------------------------------------------
// Helpers shared across tabs
// ---------------------------------------------------------------------------

static QWidget *makeSectionHeader(const QString &title, QWidget *parent)
{
    auto *w   = new QWidget(parent);
    auto *lbl = new QLabel(title, w);
    QFont f   = lbl->font();
    f.setBold(true);
    lbl->setFont(f);
    auto *line = new QFrame(w);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 6, 0, 2);
    lay->setSpacing(2);
    lay->addWidget(lbl);
    lay->addWidget(line);
    return w;
}

static QComboBox *makeQslCombo(QWidget *parent)
{
    auto *cb = new QComboBox(parent);
    cb->addItem(QStringLiteral("N — not sent / not confirmed"), QChar('N'));
    cb->addItem(QStringLiteral("Y — sent / confirmed"),        QChar('Y'));
    cb->addItem(QStringLiteral("R — requested"),               QChar('R'));
    cb->addItem(QStringLiteral("Q — queued"),                  QChar('Q'));
    cb->addItem(QStringLiteral("I — ignore"),                  QChar('I'));
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

// Wrap a form content widget in a QScrollArea.
static QScrollArea *scrolled(QWidget *content)
{
    auto *sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setWidget(content);
    return sa;
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

QsoFullEntryDialog::QsoFullEntryDialog(const Qso &qso, QWidget *parent)
    : QDialog(parent)
    , m_original(qso)
{
    const bool isNew = (qso.id < 0);
    setWindowTitle(isNew
        ? tr("New QSO — Full Entry")
        : tr("Edit QSO — %1").arg(qso.callsign));
    setMinimumSize(580, 520);
    resize(640, 580);

    auto *tabs = new QTabWidget(this);
    buildContactTab(tabs);
    buildActivityTab(tabs);
    buildPropagationTab(tabs);
    buildContestTab(tabs);
    buildSatelliteTab(tabs);
    buildNotesTab(tabs);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *root = new QVBoxLayout(this);
    root->addWidget(tabs);
    root->addWidget(buttons);

    loadQso(qso);
}

// ---------------------------------------------------------------------------
// Tab builders
// ---------------------------------------------------------------------------

void QsoFullEntryDialog::buildContactTab(QTabWidget *tabs)
{
    auto *w    = new QWidget;
    auto *form = new QFormLayout(w);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);

    // ---- Date / Time ----
    form->addRow(makeSectionHeader(tr("Date / Time"), w));

    m_datetimeOn = new QDateTimeEdit(w);
    m_datetimeOn->setTimeZone(QTimeZone::utc());
    m_datetimeOn->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    form->addRow(tr("UTC On:"), m_datetimeOn);

    auto *offRow = new QHBoxLayout;
    m_datetimeOffEn = new QCheckBox(tr("Set end time"), w);
    m_datetimeOff   = new QDateTimeEdit(w);
    m_datetimeOff->setTimeZone(QTimeZone::utc());
    m_datetimeOff->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_datetimeOff->setEnabled(false);
    connect(m_datetimeOffEn, &QCheckBox::toggled,
            m_datetimeOff, &QWidget::setEnabled);
    offRow->addWidget(m_datetimeOffEn);
    offRow->addWidget(m_datetimeOff);
    offRow->addStretch();
    form->addRow(tr("UTC Off:"), offRow);

    // ---- RF ----
    form->addRow(makeSectionHeader(tr("RF"), w));

    m_band = new QComboBox(w);
    m_band->addItem(tr("—"));
    m_band->addItems(BandPlan::bandNames());
    m_freq = new QDoubleSpinBox(w);
    m_freq->setRange(0.0, 999999.0);
    m_freq->setDecimals(6);
    m_freq->setSuffix(tr(" MHz"));
    auto *bandFreqRow = new QHBoxLayout;
    bandFreqRow->addWidget(m_band);
    bandFreqRow->addWidget(m_freq, 1);
    bandFreqRow->addStretch();
    form->addRow(tr("Band / Freq:"), bandFreqRow);

    m_bandRx = new QLineEdit(w);
    m_bandRx->setPlaceholderText(tr("—"));
    m_bandRx->setMaximumWidth(70);
    m_freqRx = new QDoubleSpinBox(w);
    m_freqRx->setRange(0.0, 999999.0);
    m_freqRx->setDecimals(6);
    m_freqRx->setSuffix(tr(" MHz"));
    auto *rxRow = new QHBoxLayout;
    rxRow->addWidget(m_bandRx);
    rxRow->addWidget(m_freqRx, 1);
    rxRow->addStretch();
    form->addRow(tr("RX Band / Freq:"), rxRow);

    m_mode = new QComboBox(w);
    m_mode->addItems(BandPlan::modeNames());
    m_submode = new QComboBox(w);
    connect(m_mode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QsoFullEntryDialog::onModeChanged);
    auto *modeRow = new QHBoxLayout;
    modeRow->addWidget(m_mode);
    modeRow->addWidget(m_submode);
    modeRow->addStretch();
    form->addRow(tr("Mode:"), modeRow);

    m_rstSent = new QLineEdit(w);  m_rstSent->setMaxLength(6);  m_rstSent->setFixedWidth(60);
    m_rstRcvd = new QLineEdit(w);  m_rstRcvd->setMaxLength(6);  m_rstRcvd->setFixedWidth(60);
    auto *rstRow = new QHBoxLayout;
    rstRow->addWidget(m_rstSent);
    rstRow->addWidget(new QLabel(QStringLiteral("/"), w));
    rstRow->addWidget(m_rstRcvd);
    rstRow->addStretch();
    form->addRow(tr("RST Sent / Rcvd:"), rstRow);

    // ---- Contact info ----
    form->addRow(makeSectionHeader(tr("Contact"), w));

    m_callsign = new QLineEdit(w);
    m_callsign->setMaxLength(15);
    form->addRow(tr("Callsign:"),        m_callsign);

    m_name = new QLineEdit(w);
    form->addRow(tr("Name:"),            m_name);

    m_contactedOp = new QLineEdit(w);
    form->addRow(tr("Operator (at stn):"), m_contactedOp);

    m_qth = new QLineEdit(w);
    form->addRow(tr("QTH:"),             m_qth);

    m_gridsquare = new QLineEdit(w);
    m_gridsquare->setMaxLength(10);
    form->addRow(tr("Grid Square:"),     m_gridsquare);

    m_country = new QLineEdit(w);
    form->addRow(tr("Country:"),         m_country);

    m_state = new QLineEdit(w);
    form->addRow(tr("State / Province:"), m_state);

    m_county = new QLineEdit(w);
    form->addRow(tr("County:"),          m_county);

    m_cont = new QLineEdit(w);
    m_cont->setMaxLength(2);
    m_cont->setFixedWidth(40);
    form->addRow(tr("Continent:"),       m_cont);

    m_dxcc = new QSpinBox(w);
    m_dxcc->setRange(0, 9999);
    m_dxcc->setSpecialValueText(tr("—"));
    form->addRow(tr("DXCC:"),            m_dxcc);

    m_cqZone = new QSpinBox(w);
    m_cqZone->setRange(0, 40);
    m_cqZone->setSpecialValueText(tr("—"));
    form->addRow(tr("CQ Zone:"),         m_cqZone);

    m_ituZone = new QSpinBox(w);
    m_ituZone->setRange(0, 90);
    m_ituZone->setSpecialValueText(tr("—"));
    form->addRow(tr("ITU Zone:"),        m_ituZone);

    m_pfx = new QLineEdit(w);
    m_pfx->setMaxLength(10);
    form->addRow(tr("Prefix:"),          m_pfx);

    m_arrlSect = new QLineEdit(w);
    m_arrlSect->setMaxLength(10);
    form->addRow(tr("ARRL Section:"),    m_arrlSect);

    m_age = new QSpinBox(w);
    m_age->setRange(0, 120);
    m_age->setSpecialValueText(tr("—"));
    form->addRow(tr("Age:"),             m_age);

    m_eqCall = new QLineEdit(w);
    m_eqCall->setMaxLength(20);
    form->addRow(tr("eQSL Callsign:"),   m_eqCall);

    // ---- My Station ----
    form->addRow(makeSectionHeader(tr("My Station"), w));

    m_stationCallsign = new QLineEdit(w);
    m_stationCallsign->setMaxLength(15);
    form->addRow(tr("Station Callsign:"), m_stationCallsign);

    m_txPwr = new QDoubleSpinBox(w);
    m_txPwr->setRange(0.0, 50000.0);
    m_txPwr->setDecimals(1);
    m_txPwr->setSuffix(tr(" W"));
    m_txPwr->setSpecialValueText(tr("—"));
    form->addRow(tr("TX Power:"),        m_txPwr);

    m_antenna = new QLineEdit(w);
    form->addRow(tr("Antenna:"),         m_antenna);

    m_rig = new QLineEdit(w);
    form->addRow(tr("Rig:"),             m_rig);

    // ---- Comment ----
    form->addRow(makeSectionHeader(tr("Comment"), w));

    m_comment = new QLineEdit(w);
    form->addRow(tr("Comment:"),         m_comment);

    // ---- QSL Status ----
    form->addRow(makeSectionHeader(tr("QSL Status"), w));

    auto makeQslRow = [&](QComboBox *&sent, QComboBox *&rcvd) {
        sent = makeQslCombo(w);
        rcvd = makeQslCombo(w);
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel(tr("Sent:"), w));
        row->addWidget(sent, 1);
        row->addWidget(new QLabel(tr("  Rcvd:"), w));
        row->addWidget(rcvd, 1);
        return row;
    };

    form->addRow(tr("LoTW:"),  makeQslRow(m_lotwSent, m_lotwRcvd));
    form->addRow(tr("eQSL:"),  makeQslRow(m_eqslSent, m_eqslRcvd));
    form->addRow(tr("QRZ:"),   makeQslRow(m_qrzSent,  m_qrzRcvd));

    // Paper QSL — sent only (no rcvd for paper in most workflows, but ADIF supports it)
    m_qslSent = makeQslCombo(w);
    m_qslRcvd = makeQslCombo(w);
    auto *paperRow = new QHBoxLayout;
    paperRow->addWidget(new QLabel(tr("Sent:"), w));
    paperRow->addWidget(m_qslSent, 1);
    paperRow->addWidget(new QLabel(tr("  Rcvd:"), w));
    paperRow->addWidget(m_qslRcvd, 1);
    form->addRow(tr("Paper QSL:"), paperRow);

    tabs->addTab(scrolled(w), tr("Contact"));
}

void QsoFullEntryDialog::buildActivityTab(QTabWidget *tabs)
{
    auto *w    = new QWidget;
    auto *form = new QFormLayout(w);

    form->addRow(makeSectionHeader(tr("DX Station Activity"), w));

    m_sig = new QLineEdit(w);
    m_sig->setMaxLength(30);
    m_sig->setPlaceholderText(tr("e.g. POTA, SOTA, WWFF"));
    form->addRow(tr("Program (SIG):"),   m_sig);

    m_sigInfo = new QLineEdit(w);
    m_sigInfo->setMaxLength(50);
    m_sigInfo->setPlaceholderText(tr("e.g. K-0001, W-1234"));
    form->addRow(tr("Reference:"),       m_sigInfo);

    form->addRow(makeSectionHeader(tr("My Activity"), w));

    m_mySig = new QLineEdit(w);
    m_mySig->setMaxLength(30);
    m_mySig->setPlaceholderText(tr("e.g. POTA, SOTA, WWFF"));
    form->addRow(tr("Program (MY_SIG):"), m_mySig);

    m_mySigInfo = new QLineEdit(w);
    m_mySigInfo->setMaxLength(50);
    m_mySigInfo->setPlaceholderText(tr("e.g. K-0001, W-1234"));
    form->addRow(tr("Reference:"),       m_mySigInfo);

    tabs->addTab(scrolled(w), tr("Activity"));
}

void QsoFullEntryDialog::buildPropagationTab(QTabWidget *tabs)
{
    auto *w    = new QWidget;
    auto *form = new QFormLayout(w);

    m_propMode = new QLineEdit(w);
    m_propMode->setPlaceholderText(tr("e.g. EME, MS, TR, AS, F2"));
    m_propMode->setMaxLength(10);
    form->addRow(tr("Propagation Mode:"), m_propMode);

    m_sfi = new QDoubleSpinBox(w);
    m_sfi->setRange(0.0, 500.0);
    m_sfi->setDecimals(1);
    m_sfi->setSpecialValueText(tr("—"));
    form->addRow(tr("Solar Flux (SFI):"), m_sfi);

    m_aIndex = new QDoubleSpinBox(w);
    m_aIndex->setRange(0.0, 400.0);
    m_aIndex->setDecimals(1);
    m_aIndex->setSpecialValueText(tr("—"));
    form->addRow(tr("A-Index:"),          m_aIndex);

    m_kIndex = new QDoubleSpinBox(w);
    m_kIndex->setRange(0.0, 9.0);
    m_kIndex->setDecimals(1);
    m_kIndex->setSpecialValueText(tr("—"));
    form->addRow(tr("K-Index:"),          m_kIndex);

    m_sunspots = new QSpinBox(w);
    m_sunspots->setRange(0, 500);
    m_sunspots->setSpecialValueText(tr("—"));
    form->addRow(tr("Sunspots:"),         m_sunspots);

    tabs->addTab(scrolled(w), tr("Propagation"));
}

void QsoFullEntryDialog::buildContestTab(QTabWidget *tabs)
{
    auto *w    = new QWidget;
    auto *form = new QFormLayout(w);

    m_contestId = new QLineEdit(w);
    m_contestId->setMaxLength(40);
    m_contestId->setPlaceholderText(tr("e.g. CQ-WW-SSB"));
    form->addRow(tr("Contest ID:"),       m_contestId);

    form->addRow(makeSectionHeader(tr("Received Exchange"), w));

    m_srx = new QSpinBox(w);
    m_srx->setRange(0, 99999);
    m_srx->setSpecialValueText(tr("—"));
    form->addRow(tr("Serial Number:"),    m_srx);

    m_srxString = new QLineEdit(w);
    m_srxString->setMaxLength(30);
    form->addRow(tr("Exchange:"),         m_srxString);

    form->addRow(makeSectionHeader(tr("Sent Exchange"), w));

    m_stx = new QSpinBox(w);
    m_stx->setRange(0, 99999);
    m_stx->setSpecialValueText(tr("—"));
    form->addRow(tr("Serial Number:"),    m_stx);

    m_stxString = new QLineEdit(w);
    m_stxString->setMaxLength(30);
    form->addRow(tr("Exchange:"),         m_stxString);

    form->addRow(makeSectionHeader(tr("Other"), w));

    m_precedence = new QLineEdit(w);
    m_precedence->setMaxLength(10);
    m_precedence->setPlaceholderText(tr("e.g. A, B, M, Q, S, U (Sweepstakes)"));
    form->addRow(tr("Precedence:"),       m_precedence);

    m_contestClass = new QLineEdit(w);
    m_contestClass->setMaxLength(10);
    form->addRow(tr("Class:"),            m_contestClass);

    m_arrlCheck = new QLineEdit(w);
    m_arrlCheck->setMaxLength(10);
    m_arrlCheck->setPlaceholderText(tr("e.g. 73 (last 2 digits of license year)"));
    form->addRow(tr("ARRL Check:"),       m_arrlCheck);

    tabs->addTab(scrolled(w), tr("Contest"));
}

void QsoFullEntryDialog::buildSatelliteTab(QTabWidget *tabs)
{
    auto *w    = new QWidget;
    auto *form = new QFormLayout(w);

    form->addRow(makeSectionHeader(tr("Satellite"), w));

    m_satName = new QLineEdit(w);
    m_satName->setMaxLength(20);
    m_satName->setPlaceholderText(tr("e.g. SO-50, AO-91"));
    form->addRow(tr("Satellite Name:"),  m_satName);

    m_satMode = new QLineEdit(w);
    m_satMode->setMaxLength(10);
    m_satMode->setPlaceholderText(tr("e.g. V/U, U/V, L/U"));
    form->addRow(tr("Satellite Mode:"),  m_satMode);

    form->addRow(makeSectionHeader(tr("Meteor Scatter"), w));

    m_maxBursts = new QDoubleSpinBox(w);
    m_maxBursts->setRange(0.0, 9999.0);
    m_maxBursts->setDecimals(1);
    m_maxBursts->setSpecialValueText(tr("—"));
    form->addRow(tr("Max Bursts:"),      m_maxBursts);

    m_nrBursts = new QSpinBox(w);
    m_nrBursts->setRange(0, 9999);
    m_nrBursts->setSpecialValueText(tr("—"));
    form->addRow(tr("Nr Bursts:"),       m_nrBursts);

    m_nrPings = new QSpinBox(w);
    m_nrPings->setRange(0, 9999);
    m_nrPings->setSpecialValueText(tr("—"));
    form->addRow(tr("Nr Pings:"),        m_nrPings);

    m_msShower = new QLineEdit(w);
    m_msShower->setMaxLength(20);
    m_msShower->setPlaceholderText(tr("e.g. Perseids, Leonids"));
    form->addRow(tr("MS Shower:"),       m_msShower);

    tabs->addTab(scrolled(w), tr("Satellite"));
}

void QsoFullEntryDialog::buildNotesTab(QTabWidget *tabs)
{
    auto *w   = new QWidget;
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(6, 6, 6, 6);

    m_notes = new QPlainTextEdit(w);
    m_notes->setPlaceholderText(tr("Extended notes — any length, not exported in comment field"));

    m_qsoComplete = new QLineEdit(w);
    m_qsoComplete->setMaxLength(3);
    m_qsoComplete->setPlaceholderText(tr("Y, N, NIL, ??"));
    m_qsoComplete->setFixedWidth(60);

    auto *completeRow = new QHBoxLayout;
    completeRow->addWidget(new QLabel(tr("QSO Complete:"), w));
    completeRow->addWidget(m_qsoComplete);
    completeRow->addStretch();

    lay->addWidget(new QLabel(tr("Notes:"), w));
    lay->addWidget(m_notes, 1);
    lay->addLayout(completeRow);

    tabs->addTab(w, tr("Notes"));
}

// ---------------------------------------------------------------------------
// Slot
// ---------------------------------------------------------------------------

void QsoFullEntryDialog::onModeChanged()
{
    const QStringList subs = BandPlan::submodesForMode(m_mode->currentText());
    m_submode->blockSignals(true);
    m_submode->clear();
    m_submode->setEnabled(!subs.isEmpty());
    if (!subs.isEmpty())
        m_submode->addItems(subs);
    m_submode->blockSignals(false);
}

// ---------------------------------------------------------------------------
// Load / save
// ---------------------------------------------------------------------------

void QsoFullEntryDialog::loadQso(const Qso &q)
{
    // Date/Time
    m_datetimeOn->setDateTime(q.datetimeOn.isValid()
        ? q.datetimeOn.toUTC()
        : QDateTime::currentDateTimeUtc());
    if (q.datetimeOff.isValid()) {
        m_datetimeOffEn->setChecked(true);
        m_datetimeOff->setDateTime(q.datetimeOff.toUTC());
    } else {
        m_datetimeOffEn->setChecked(false);
        m_datetimeOff->setDateTime(QDateTime::currentDateTimeUtc());
    }

    // RF
    const int bIdx = m_band->findText(q.band, Qt::MatchFixedString);
    m_band->setCurrentIndex(bIdx >= 0 ? bIdx : 0);
    m_freq->setValue(q.freq);
    m_bandRx->setText(q.bandRx);
    m_freqRx->setValue(q.freqRx.value_or(0.0));

    const int mIdx = m_mode->findText(q.mode, Qt::MatchFixedString);
    if (mIdx >= 0) m_mode->setCurrentIndex(mIdx);
    onModeChanged();
    if (!q.submode.isEmpty()) {
        const int sIdx = m_submode->findText(q.submode, Qt::MatchFixedString);
        if (sIdx >= 0) m_submode->setCurrentIndex(sIdx);
    }

    m_rstSent->setText(q.rstSent);
    m_rstRcvd->setText(q.rstRcvd);

    // Contact
    m_callsign->setText(q.callsign);
    m_name->setText(q.name);
    m_contactedOp->setText(q.contactedOp);
    m_qth->setText(q.qth);
    m_gridsquare->setText(q.gridsquare);
    m_country->setText(q.country);
    m_state->setText(q.state);
    m_county->setText(q.county);
    m_cont->setText(q.cont);
    m_dxcc->setValue(q.dxcc);
    m_cqZone->setValue(q.cqZone);
    m_ituZone->setValue(q.ituZone);
    m_pfx->setText(q.pfx);
    m_arrlSect->setText(q.arrlSect);
    m_age->setValue(q.age.value_or(0));
    m_eqCall->setText(q.eqCall);

    // My station
    m_stationCallsign->setText(q.stationCallsign);
    m_txPwr->setValue(q.txPwr.value_or(0.0));
    m_antenna->setText(q.antenna);
    m_rig->setText(q.rig);
    m_comment->setText(q.comment);

    // QSL
    setQslStatus(m_lotwSent, q.lotwQslSent);
    setQslStatus(m_lotwRcvd, q.lotwQslRcvd);
    setQslStatus(m_eqslSent, q.eqslQslSent);
    setQslStatus(m_eqslRcvd, q.eqslQslRcvd);
    setQslStatus(m_qrzSent,  q.qrzQslSent);
    setQslStatus(m_qrzRcvd,  q.qrzQslRcvd);
    setQslStatus(m_qslSent,  q.qslSent);
    setQslStatus(m_qslRcvd,  q.qslRcvd);

    // Activity
    m_sig->setText(q.sig);
    m_sigInfo->setText(q.sigInfo);
    m_mySig->setText(q.mySig);
    m_mySigInfo->setText(q.mySigInfo);

    // Propagation
    m_propMode->setText(q.propMode);
    m_sfi->setValue(q.sfi.value_or(0.0));
    m_aIndex->setValue(q.aIndex.value_or(0.0));
    m_kIndex->setValue(q.kIndex.value_or(0.0));
    m_sunspots->setValue(q.sunspots.value_or(0));

    // Contest
    m_contestId->setText(q.contestId);
    m_srx->setValue(q.srx.value_or(0));
    m_srxString->setText(q.srxString);
    m_stx->setValue(q.stx.value_or(0));
    m_stxString->setText(q.stxString);
    m_precedence->setText(q.precedence);
    m_contestClass->setText(q.contestClass);
    m_arrlCheck->setText(q.arrlCheck);

    // Satellite
    m_satName->setText(q.satName);
    m_satMode->setText(q.satMode);
    m_maxBursts->setValue(q.maxBursts.value_or(0.0));
    m_nrBursts->setValue(q.nrBursts.value_or(0));
    m_nrPings->setValue(q.nrPings.value_or(0));
    m_msShower->setText(q.msShower);

    // Notes
    m_notes->setPlainText(q.notes);
    m_qsoComplete->setText(q.qsoComplete);
}

Qso QsoFullEntryDialog::qso() const
{
    Qso q = m_original;

    // Date/Time
    q.datetimeOn  = m_datetimeOn->dateTime().toUTC();
    q.datetimeOff = m_datetimeOffEn->isChecked()
        ? m_datetimeOff->dateTime().toUTC()
        : QDateTime{};

    // RF
    q.band    = m_band->currentIndex() > 0 ? m_band->currentText() : QString();
    q.freq    = m_freq->value();
    q.bandRx  = m_bandRx->text().trimmed();
    q.freqRx  = m_freqRx->value() > 0.0
        ? std::optional<double>(m_freqRx->value()) : std::nullopt;
    q.mode    = m_mode->currentText();
    q.submode = m_submode->isEnabled() ? m_submode->currentText() : QString();
    q.rstSent = m_rstSent->text().trimmed();
    q.rstRcvd = m_rstRcvd->text().trimmed();

    // Contact
    q.callsign    = m_callsign->text().toUpper().trimmed();
    q.name        = m_name->text().trimmed();
    q.contactedOp = m_contactedOp->text().trimmed();
    q.qth         = m_qth->text().trimmed();
    q.gridsquare  = m_gridsquare->text().toUpper().trimmed();
    q.country     = m_country->text().trimmed();
    q.state       = m_state->text().trimmed();
    q.county      = m_county->text().trimmed();
    q.cont        = m_cont->text().toUpper().trimmed();
    q.dxcc        = m_dxcc->value();
    q.cqZone      = m_cqZone->value();
    q.ituZone     = m_ituZone->value();
    q.pfx         = m_pfx->text().toUpper().trimmed();
    q.arrlSect    = m_arrlSect->text().toUpper().trimmed();
    q.age         = m_age->value() > 0 ? std::optional<int>(m_age->value()) : std::nullopt;
    q.eqCall      = m_eqCall->text().toUpper().trimmed();

    // My station
    q.stationCallsign = m_stationCallsign->text().toUpper().trimmed();
    const double pwr  = m_txPwr->value();
    q.txPwr           = pwr > 0.0 ? std::optional<double>(pwr) : std::nullopt;
    q.antenna         = m_antenna->text().trimmed();
    q.rig             = m_rig->text().trimmed();
    q.comment         = m_comment->text().trimmed();

    // QSL
    q.lotwQslSent  = getQslStatus(m_lotwSent);
    q.lotwQslRcvd  = getQslStatus(m_lotwRcvd);
    q.eqslQslSent  = getQslStatus(m_eqslSent);
    q.eqslQslRcvd  = getQslStatus(m_eqslRcvd);
    q.qrzQslSent   = getQslStatus(m_qrzSent);
    q.qrzQslRcvd   = getQslStatus(m_qrzRcvd);
    q.qslSent      = getQslStatus(m_qslSent);
    q.qslRcvd      = getQslStatus(m_qslRcvd);

    // Activity
    q.sig       = m_sig->text().toUpper().trimmed();
    q.sigInfo   = m_sigInfo->text().toUpper().trimmed();
    q.mySig     = m_mySig->text().toUpper().trimmed();
    q.mySigInfo = m_mySigInfo->text().toUpper().trimmed();

    // Propagation
    q.propMode = m_propMode->text().toUpper().trimmed();
    q.sfi      = m_sfi->value()      > 0.0 ? std::optional<double>(m_sfi->value())      : std::nullopt;
    q.aIndex   = m_aIndex->value()   > 0.0 ? std::optional<double>(m_aIndex->value())   : std::nullopt;
    q.kIndex   = m_kIndex->value()   > 0.0 ? std::optional<double>(m_kIndex->value())   : std::nullopt;
    q.sunspots = m_sunspots->value() > 0   ? std::optional<int>(m_sunspots->value())    : std::nullopt;

    // Contest
    q.contestId    = m_contestId->text().toUpper().trimmed();
    q.srx          = m_srx->value() > 0 ? std::optional<int>(m_srx->value()) : std::nullopt;
    q.srxString    = m_srxString->text().trimmed();
    q.stx          = m_stx->value() > 0 ? std::optional<int>(m_stx->value()) : std::nullopt;
    q.stxString    = m_stxString->text().trimmed();
    q.precedence   = m_precedence->text().toUpper().trimmed();
    q.contestClass = m_contestClass->text().toUpper().trimmed();
    q.arrlCheck    = m_arrlCheck->text().trimmed();

    // Satellite
    q.satName   = m_satName->text().trimmed();
    q.satMode   = m_satMode->text().toUpper().trimmed();
    q.maxBursts = m_maxBursts->value() > 0.0 ? std::optional<double>(m_maxBursts->value()) : std::nullopt;
    q.nrBursts  = m_nrBursts->value()  > 0   ? std::optional<int>(m_nrBursts->value())     : std::nullopt;
    q.nrPings   = m_nrPings->value()   > 0   ? std::optional<int>(m_nrPings->value())      : std::nullopt;
    q.msShower  = m_msShower->text().trimmed();

    // Notes
    q.notes       = m_notes->toPlainText().trimmed();
    q.qsoComplete = m_qsoComplete->text().toUpper().trimmed();

    return q;
}
