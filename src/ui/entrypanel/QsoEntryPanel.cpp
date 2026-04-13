#include "QsoEntryPanel.h"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QTimeZone>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>

#include "app/settings/Settings.h"
#include "core/BandPlan.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

QsoEntryPanel::QsoEntryPanel(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(true);

    // -----------------------------------------------------------------------
    // Row 1: Date/Time | Callsign | Band | Freq | Mode | Submode
    // -----------------------------------------------------------------------
    m_dateTime = new QDateTimeEdit(this);
    m_dateTime->setTimeZone(QTimeZone::utc());
    m_dateTime->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    m_dateTime->setFixedWidth(168);
    m_dateTime->setToolTip(tr("QSO start time (UTC)"));

    m_nowBtn = new QPushButton(tr("Now"), this);
    m_nowBtn->setFixedWidth(42);
    m_nowBtn->setToolTip(tr("Set to current UTC time"));
    connect(m_nowBtn, &QPushButton::clicked, this, &QsoEntryPanel::onNowClicked);

    m_callsign = new QLineEdit(this);
    m_callsign->setPlaceholderText(tr("Callsign"));
    m_callsign->setMaxLength(20);
    m_callsign->setMinimumWidth(100);
    m_callsign->setToolTip(tr("Contacted station callsign"));
    connect(m_callsign, &QLineEdit::textChanged, this, [this](const QString &t) {
        m_callsign->setText(t.toUpper());
        m_callsign->blockSignals(true);  // prevent infinite loop from setText
        const int pos = m_callsign->cursorPosition();
        m_callsign->setText(t.toUpper());
        m_callsign->setCursorPosition(pos);
        m_callsign->blockSignals(false);
    });

    m_band = new QComboBox(this);
    m_band->setFixedWidth(70);
    m_band->addItem(tr("—"));  // "not set" placeholder
    m_band->addItems(BandPlan::bandNames());
    m_band->setToolTip(tr("Band"));
    connect(m_band, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QsoEntryPanel::onBandChanged);

    m_freq = new QDoubleSpinBox(this);
    m_freq->setRange(0.0, 999999.0);
    m_freq->setDecimals(6);
    m_freq->setSuffix(tr(" MHz"));
    m_freq->setFixedWidth(140);
    m_freq->setToolTip(tr("Frequency (MHz)"));
    connect(m_freq, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &QsoEntryPanel::onFreqChanged);

    m_mode = new QComboBox(this);
    m_mode->setFixedWidth(90);
    m_mode->addItems(BandPlan::modeNames());
    m_mode->setToolTip(tr("Mode"));
    connect(m_mode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QsoEntryPanel::onModeChanged);

    m_submode = new QComboBox(this);
    m_submode->setFixedWidth(70);
    m_submode->setToolTip(tr("Submode"));

    // -----------------------------------------------------------------------
    // Row 2: RST | Name | Grid | TX Power
    // -----------------------------------------------------------------------
    m_rstSent = new QLineEdit(this);
    m_rstSent->setFixedWidth(48);
    m_rstSent->setPlaceholderText(tr("Snt"));
    m_rstSent->setToolTip(tr("RST sent"));
    m_rstSent->setMaxLength(6);

    m_rstRcvd = new QLineEdit(this);
    m_rstRcvd->setFixedWidth(48);
    m_rstRcvd->setPlaceholderText(tr("Rcvd"));
    m_rstRcvd->setToolTip(tr("RST received"));
    m_rstRcvd->setMaxLength(6);

    // Log on Enter from RST Rcvd — most common fast-entry workflow
    connect(m_rstRcvd, &QLineEdit::returnPressed, this, &QsoEntryPanel::onLogClicked);

    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(tr("Name"));
    m_name->setMinimumWidth(80);
    m_name->setToolTip(tr("Operator name"));

    m_grid = new QLineEdit(this);
    m_grid->setFixedWidth(68);
    m_grid->setPlaceholderText(tr("Grid"));
    m_grid->setMaxLength(10);
    m_grid->setToolTip(tr("Maidenhead grid square"));
    connect(m_grid, &QLineEdit::textChanged, this, [this](const QString &) {
        m_grid->blockSignals(true);
        const int pos = m_grid->cursorPosition();
        m_grid->setText(m_grid->text().toUpper());
        m_grid->setCursorPosition(pos);
        m_grid->blockSignals(false);
    });

    m_txPwr = new QDoubleSpinBox(this);
    m_txPwr->setRange(0.0, 2000.0);
    m_txPwr->setDecimals(1);
    m_txPwr->setSuffix(tr(" W"));
    m_txPwr->setFixedWidth(80);
    m_txPwr->setToolTip(tr("TX power (Watts)"));

    // -----------------------------------------------------------------------
    // Row 3: SIG / SIG_INFO | Comment | Log | Clear
    // -----------------------------------------------------------------------
    m_sig = new QLineEdit(this);
    m_sig->setFixedWidth(56);
    m_sig->setPlaceholderText(tr("SIG"));
    m_sig->setToolTip(tr("Special activity (e.g. POTA, SOTA)"));
    connect(m_sig, &QLineEdit::textChanged, this, [this](const QString &) {
        m_sig->blockSignals(true);
        const int pos = m_sig->cursorPosition();
        m_sig->setText(m_sig->text().toUpper());
        m_sig->setCursorPosition(pos);
        m_sig->blockSignals(false);
    });

    m_sigInfo = new QLineEdit(this);
    m_sigInfo->setFixedWidth(90);
    m_sigInfo->setPlaceholderText(tr("Reference"));
    m_sigInfo->setToolTip(tr("Activity reference (e.g. K-0001)"));
    connect(m_sigInfo, &QLineEdit::textChanged, this, [this](const QString &) {
        m_sigInfo->blockSignals(true);
        const int pos = m_sigInfo->cursorPosition();
        m_sigInfo->setText(m_sigInfo->text().toUpper());
        m_sigInfo->setCursorPosition(pos);
        m_sigInfo->blockSignals(false);
    });

    m_comment = new QLineEdit(this);
    m_comment->setPlaceholderText(tr("Comment"));
    m_comment->setToolTip(tr("Contact comment"));

    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_logBtn = new QPushButton(tr("Log Contact"), this);
    m_logBtn->setDefault(true);
    m_logBtn->setFixedWidth(100);
    QFont boldFont = m_logBtn->font();
    boldFont.setBold(true);
    m_logBtn->setFont(boldFont);
    connect(m_logBtn, &QPushButton::clicked, this, &QsoEntryPanel::onLogClicked);

    m_clearBtn = new QPushButton(tr("Clear"), this);
    m_clearBtn->setFixedWidth(60);
    connect(m_clearBtn, &QPushButton::clicked, this, &QsoEntryPanel::clearForm);

    // -----------------------------------------------------------------------
    // Layout
    // -----------------------------------------------------------------------
    auto makeSep = []() -> QFrame * {
        auto *f = new QFrame;
        f->setFrameShape(QFrame::VLine);
        f->setFrameShadow(QFrame::Sunken);
        return f;
    };

    // Row 1
    auto *row1 = new QHBoxLayout;
    row1->addWidget(new QLabel(tr("UTC:"), this));
    row1->addWidget(m_dateTime);
    row1->addWidget(m_nowBtn);
    row1->addWidget(makeSep());
    row1->addWidget(new QLabel(tr("Call:"), this));
    row1->addWidget(m_callsign, 1);
    row1->addWidget(makeSep());
    row1->addWidget(new QLabel(tr("Band:"), this));
    row1->addWidget(m_band);
    row1->addWidget(new QLabel(tr("Freq:"), this));
    row1->addWidget(m_freq);
    row1->addWidget(makeSep());
    row1->addWidget(new QLabel(tr("Mode:"), this));
    row1->addWidget(m_mode);
    row1->addWidget(m_submode);

    // Row 2
    auto *row2 = new QHBoxLayout;
    row2->addWidget(new QLabel(tr("RST S/R:"), this));
    row2->addWidget(m_rstSent);
    row2->addWidget(m_rstRcvd);
    row2->addWidget(makeSep());
    row2->addWidget(new QLabel(tr("Name:"), this));
    row2->addWidget(m_name, 1);
    row2->addWidget(new QLabel(tr("Grid:"), this));
    row2->addWidget(m_grid);
    row2->addWidget(makeSep());
    row2->addWidget(new QLabel(tr("PWR:"), this));
    row2->addWidget(m_txPwr);

    // Row 3
    auto *row3 = new QHBoxLayout;
    row3->addWidget(new QLabel(tr("Activity:"), this));
    row3->addWidget(m_sig);
    row3->addWidget(m_sigInfo);
    row3->addWidget(makeSep());
    row3->addWidget(new QLabel(tr("Comment:"), this));
    row3->addWidget(m_comment, 1);
    row3->addWidget(m_statusLabel, 1);
    row3->addWidget(m_logBtn);
    row3->addWidget(m_clearBtn);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(6, 4, 6, 4);
    root->setSpacing(4);
    root->addLayout(row1);
    root->addLayout(row2);
    root->addLayout(row3);

    // -----------------------------------------------------------------------
    // Tab order: callsign → band → freq → mode → RST Sent → RST Rcvd → name → grid → comment → log
    // -----------------------------------------------------------------------
    setTabOrder(m_callsign, m_band);
    setTabOrder(m_band, m_freq);
    setTabOrder(m_freq, m_mode);
    setTabOrder(m_mode, m_rstSent);
    setTabOrder(m_rstSent, m_rstRcvd);
    setTabOrder(m_rstRcvd, m_name);
    setTabOrder(m_name, m_grid);
    setTabOrder(m_grid, m_comment);
    setTabOrder(m_comment, m_logBtn);

    // Initialise to a clean state
    clearForm();
    // Trigger initial submode population
    onModeChanged(m_mode->currentIndex());
}

// ---------------------------------------------------------------------------
// Public API for radio backends
// ---------------------------------------------------------------------------

void QsoEntryPanel::setRadioFreq(double freqMhz)
{
    setFreqSilently(freqMhz);
    const QString band = BandPlan::freqToBand(freqMhz);
    if (!band.isEmpty())
        setBandSilently(band);
}

void QsoEntryPanel::setRadioMode(const QString &mode, const QString &submode)
{
    const int idx = m_mode->findText(mode, Qt::MatchFixedString);
    if (idx >= 0) {
        m_suppressBandFreqSignals = true;
        m_mode->setCurrentIndex(idx);
        m_suppressBandFreqSignals = false;
        onModeChanged(idx);
    }
    if (!submode.isEmpty()) {
        const int sidx = m_submode->findText(submode, Qt::MatchFixedString);
        if (sidx >= 0) m_submode->setCurrentIndex(sidx);
    }
}

void QsoEntryPanel::setDxCall(const QString &call)
{
    m_callsign->setText(call.toUpper());
}

void QsoEntryPanel::setDxGrid(const QString &grid)
{
    m_grid->setText(grid.toUpper());
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void QsoEntryPanel::onNowClicked()
{
    m_dateTime->setDateTime(QDateTime::currentDateTimeUtc());
}

void QsoEntryPanel::onModeChanged(int /*index*/)
{
    const QString mode = m_mode->currentText();

    // Update submode list
    const QStringList subs = BandPlan::submodesForMode(mode);
    m_submode->blockSignals(true);
    m_submode->clear();
    if (subs.isEmpty()) {
        m_submode->setEnabled(false);
    } else {
        m_submode->setEnabled(true);
        m_submode->addItems(subs);
    }
    m_submode->blockSignals(false);

    // Only overwrite RST if currently empty or still the old default
    if (m_suppressBandFreqSignals) return;
    updateRstDefaults();
}

void QsoEntryPanel::onFreqChanged(double freqMhz)
{
    if (m_suppressBandFreqSignals || freqMhz == 0.0) return;

    const QString band = BandPlan::freqToBand(freqMhz);
    if (!band.isEmpty())
        setBandSilently(band);
}

void QsoEntryPanel::onBandChanged(int index)
{
    if (m_suppressBandFreqSignals || index == 0) return;  // index 0 = "—"

    const QString band = m_band->currentText();
    const double defFreq = BandPlan::bandDefaultFreq(band);
    if (defFreq > 0.0)
        setFreqSilently(defFreq);
}

void QsoEntryPanel::onLogClicked()
{
    if (!validate()) return;

    Qso qso = buildQso();
    applyStationDefaults(qso);
    emit qsoReady(qso);

    // Retain band/mode/power/station fields; clear contact-specific fields
    const QString band    = m_band->currentText();
    const int     modeIdx = m_mode->currentIndex();
    const double  freq    = m_freq->value();
    const double  pwr     = m_txPwr->value();

    clearForm();

    // Restore sticky fields
    setBandSilently(band);
    setFreqSilently(freq);
    m_suppressBandFreqSignals = true;
    m_mode->setCurrentIndex(modeIdx);
    m_suppressBandFreqSignals = false;
    onModeChanged(modeIdx);
    m_txPwr->setValue(pwr);

    m_statusLabel->setText(tr("Logged: %1").arg(qso.callsign));
    m_callsign->setFocus();
    m_callsign->selectAll();
}

void QsoEntryPanel::clearForm()
{
    m_dateTime->setDateTime(QDateTime::currentDateTimeUtc());
    m_callsign->clear();
    m_rstSent->clear();
    m_rstRcvd->clear();
    m_name->clear();
    m_grid->clear();
    m_sig->clear();
    m_sigInfo->clear();
    m_comment->clear();
    m_statusLabel->clear();

    // Load equipment defaults from settings
    m_txPwr->setValue(Settings::instance().equipmentTxPwr());

    // Set mode to SSB if not already set (first run)
    if (m_mode->currentIndex() < 0)
        m_mode->setCurrentText("SSB");

    updateRstDefaults();
    m_callsign->setFocus();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool QsoEntryPanel::validate() const
{
    if (m_callsign->text().trimmed().isEmpty()) {
        m_callsign->setFocus();
        m_callsign->setStyleSheet("border: 1px solid red");
        QTimer::singleShot(1500, m_callsign, [this]() {
            m_callsign->setStyleSheet({});
        });
        return false;
    }
    if (m_band->currentIndex() == 0 && m_freq->value() == 0.0) {
        m_band->setFocus();
        return false;
    }
    return true;
}

Qso QsoEntryPanel::buildQso() const
{
    Qso q;
    q.datetimeOn  = m_dateTime->dateTime().toUTC();
    q.callsign    = m_callsign->text().toUpper().trimmed();

    const QString band = (m_band->currentIndex() > 0) ? m_band->currentText() : QString();
    q.band        = band;
    q.freq        = m_freq->value();
    q.mode        = m_mode->currentText();
    q.submode     = m_submode->isEnabled() ? m_submode->currentText() : QString();

    q.rstSent     = m_rstSent->text().trimmed();
    q.rstRcvd     = m_rstRcvd->text().trimmed();
    q.name        = m_name->text().trimmed();
    q.gridsquare  = m_grid->text().toUpper().trimmed();
    q.txPwr       = m_txPwr->value() > 0 ? std::optional<double>(m_txPwr->value()) : std::nullopt;
    q.sig         = m_sig->text().toUpper().trimmed();
    q.sigInfo     = m_sigInfo->text().toUpper().trimmed();
    q.comment     = m_comment->text().trimmed();
    return q;
}

void QsoEntryPanel::applyStationDefaults(Qso &q) const
{
    const Settings &s = Settings::instance();
    q.stationCallsign = s.stationCallsign();
    q.myName          = s.stationName();
    q.myGridsquare    = s.stationGridsquare();
    q.myCity          = s.stationCity();
    q.myState         = s.stationState();
    q.myCountry       = s.stationCountry();
    q.myDxcc          = s.stationDxcc();
    q.myCqZone        = s.stationCqZone();
    q.myItuZone       = s.stationItuZone();
    q.myLat           = s.stationLat();
    q.myLon           = s.stationLon();
    if (!q.txPwr.has_value() || *q.txPwr == 0.0) {
        const double pwr = s.equipmentTxPwr();
        if (pwr > 0) q.txPwr = pwr;
    }
    if (q.rig.isEmpty())    q.rig     = s.equipmentRig();
    if (q.antenna.isEmpty()) q.antenna = s.equipmentAntenna();
}

void QsoEntryPanel::updateRstDefaults()
{
    const QString def = BandPlan::modeDefaultRst(m_mode->currentText());
    if (m_rstSent->text().isEmpty()) m_rstSent->setText(def);
    if (m_rstRcvd->text().isEmpty()) m_rstRcvd->setText(def);
}

void QsoEntryPanel::setFreqSilently(double freqMhz)
{
    m_suppressBandFreqSignals = true;
    m_freq->setValue(freqMhz);
    m_suppressBandFreqSignals = false;
}

void QsoEntryPanel::setBandSilently(const QString &band)
{
    m_suppressBandFreqSignals = true;
    const int idx = m_band->findText(band, Qt::MatchFixedString);
    if (idx >= 0) m_band->setCurrentIndex(idx);
    m_suppressBandFreqSignals = false;
}
