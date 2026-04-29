// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "QsoQuickEntryPanel.h"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialog>
#include <QMouseEvent>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTimer>
#include <QTimeZone>
#include <QUrl>
#include <QVBoxLayout>

#include "app/settings/Settings.h"
#include "core/BandPlan.h"
#include "lookup/CallsignLookupResult.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

QsoQuickEntryPanel::QsoQuickEntryPanel(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(true);

    // -----------------------------------------------------------------------
    // Left column widgets
    // -----------------------------------------------------------------------

    m_dateTime = new QDateTimeEdit(this);
    m_dateTime->setTimeZone(QTimeZone::utc());
    m_dateTime->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_dateTime->setFixedWidth(168);
    m_dateTime->setToolTip(tr("QSO start time (UTC)"));

    m_nowBtn = new QPushButton(tr("Now"), this);
    m_nowBtn->setFixedWidth(42);
    m_nowBtn->setToolTip(tr("Set to current UTC time"));
    connect(m_nowBtn, &QPushButton::clicked, this, &QsoQuickEntryPanel::onNowClicked);

    m_callsign = new QLineEdit(this);
    m_callsign->setPlaceholderText(tr("Callsign"));
    m_callsign->setMaxLength(15);   // AAA/BB1CCC/EEE = 14 chars; 15 covers edge cases
    m_callsign->setFixedWidth(125);
    m_callsign->setToolTip(tr("Contacted station callsign"));
    connect(m_callsign, &QLineEdit::textChanged, this, [this](const QString &t) {
        const QString upper = t.toUpper();
        if (upper != t) {
            const QSignalBlocker blocker(m_callsign);
            const int pos = m_callsign->cursorPosition();
            m_callsign->setText(upper);
            m_callsign->setCursorPosition(pos);
        }
        emit callsignChanged(upper);
    });

    m_band = new QComboBox(this);
    m_band->setFixedWidth(70);
    m_band->addItem(tr("—"));  // em-dash placeholder meaning "not set"
    m_band->addItems(BandPlan::bandNames());
    m_band->setToolTip(tr("Band"));
    connect(m_band, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QsoQuickEntryPanel::onBandChanged);

    m_freq = new QDoubleSpinBox(this);
    m_freq->setRange(0.0, 999999.0);
    m_freq->setDecimals(6);
    m_freq->setSuffix(tr(" MHz"));
    m_freq->setFixedWidth(140);
    m_freq->setToolTip(tr("Frequency (MHz)"));
    connect(m_freq, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &QsoQuickEntryPanel::onFreqChanged);

    m_mode = new QComboBox(this);
    m_mode->setFixedWidth(90);
    m_mode->addItems(BandPlan::modeNames());
    m_mode->setToolTip(tr("Mode"));
    connect(m_mode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QsoQuickEntryPanel::onModeChanged);

    m_submode = new QComboBox(this);
    m_submode->setFixedWidth(80);
    m_submode->setToolTip(tr("Submode"));

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
    connect(m_rstRcvd, &QLineEdit::returnPressed,
            this, &QsoQuickEntryPanel::onLogClicked);

    m_comment = new QLineEdit(this);
    m_comment->setPlaceholderText(tr("Comment"));
    m_comment->setToolTip(tr("Contact comment"));

    m_fullEntryBtn = new QPushButton(tr("Full Entry…"), this);
    m_fullEntryBtn->setToolTip(tr("Open full entry form for this contact"));
    connect(m_fullEntryBtn, &QPushButton::clicked, this, [this]() {
        emit fullEntryRequested(buildQso());
    });

    m_logBtn = new QPushButton(tr("Log Contact"), this);
    m_logBtn->setDefault(true);
    m_logBtn->setFixedWidth(100);
    QFont boldFont = m_logBtn->font();
    boldFont.setBold(true);
    m_logBtn->setFont(boldFont);
    connect(m_logBtn, &QPushButton::clicked,
            this, &QsoQuickEntryPanel::onLogClicked);

    m_clearBtn = new QPushButton(tr("Clear"), this);
    m_clearBtn->setFixedWidth(60);
    connect(m_clearBtn, &QPushButton::clicked,
            this, &QsoQuickEntryPanel::clearForm);

    // -----------------------------------------------------------------------
    // Left column layout
    // -----------------------------------------------------------------------
    auto makeSep = [this]() -> QFrame * {
        auto *f = new QFrame(this);
        f->setFrameShape(QFrame::VLine);
        f->setFrameShadow(QFrame::Sunken);
        return f;
    };

    // Row 1: datetime + callsign + RST  (the fields typed most often in sequence)
    auto *row1 = new QHBoxLayout;
    row1->addWidget(new QLabel(tr("UTC:"), this));
    row1->addWidget(m_dateTime);
    row1->addWidget(m_nowBtn);
    row1->addWidget(makeSep());
    row1->addWidget(new QLabel(tr("Call:"), this));
    row1->addWidget(m_callsign);
    row1->addWidget(makeSep());
    row1->addWidget(new QLabel(tr("RST S/R:"), this));
    row1->addWidget(m_rstSent);
    row1->addWidget(m_rstRcvd);
    row1->addStretch();

    // Row 2: band / freq / mode  (usually auto-filled from radio)
    auto *row2 = new QHBoxLayout;
    row2->addWidget(new QLabel(tr("Band:"), this));
    row2->addWidget(m_band);
    row2->addWidget(new QLabel(tr("Freq:"), this));
    row2->addWidget(m_freq);
    row2->addWidget(makeSep());
    row2->addWidget(new QLabel(tr("Mode:"), this));
    row2->addWidget(m_mode);
    row2->addWidget(m_submode);
    row2->addStretch();

    // Row 3: comment + action buttons
    auto *row3 = new QHBoxLayout;
    row3->addWidget(new QLabel(tr("Comment:"), this));
    row3->addWidget(m_comment, 1);
    row3->addWidget(makeSep());
    row3->addWidget(m_fullEntryBtn);
    row3->addWidget(m_logBtn);
    row3->addWidget(m_clearBtn);

    auto *leftForm = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftForm);
    leftLayout->setContentsMargins(6, 4, 6, 4);
    leftLayout->setSpacing(4);
    leftLayout->addLayout(row1);
    leftLayout->addLayout(row2);
    leftLayout->addLayout(row3);

    setTabOrder(m_callsign, m_rstSent);
    setTabOrder(m_rstSent,  m_rstRcvd);
    setTabOrder(m_rstRcvd,  m_band);
    setTabOrder(m_band,     m_freq);
    setTabOrder(m_freq,     m_mode);
    setTabOrder(m_mode,     m_comment);
    setTabOrder(m_comment,  m_logBtn);

    // -----------------------------------------------------------------------
    // Right column — context panel (lookup + previous QSOs)
    // -----------------------------------------------------------------------
    auto makeDivider = [this]() -> QFrame * {
        auto *f = new QFrame(this);
        f->setFrameShape(QFrame::HLine);
        f->setFrameShadow(QFrame::Sunken);
        return f;
    };

    auto makeSectionHeader = [this](const QString &text) -> QLabel * {
        auto *lbl = new QLabel(text, this);
        QFont f = lbl->font();
        f.setBold(true);
        f.setPointSize(f.pointSize() - 1);
        lbl->setFont(f);
        return lbl;
    };

    m_nam = new QNetworkAccessManager(this);

    m_contactImage = new QLabel(this);
    m_contactImage->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_contactImage->setVisible(false);
    m_contactImage->setCursor(Qt::PointingHandCursor);
    m_contactImage->setToolTip(tr("Click to view full size"));
    m_contactImage->installEventFilter(this);

    m_lookupLabel = new QLabel(tr("Enter a callsign to look up station info."), this);
    m_lookupLabel->setWordWrap(true);
    m_lookupLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_prevQsosHeader = new QLabel(tr("Previous QSOs"), this);
    {
        QFont f = m_prevQsosHeader->font();
        f.setBold(true);
        f.setPointSize(f.pointSize() - 1);
        m_prevQsosHeader->setFont(f);
    }

    m_prevQsosList = new QListWidget(this);
    m_prevQsosList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_prevQsosList->setSelectionMode(QAbstractItemView::NoSelection);
    m_prevQsosList->setFocusPolicy(Qt::NoFocus);
    m_prevQsosList->setFrameShape(QFrame::NoFrame);
    m_prevQsosList->addItem(tr("Enter a callsign above."));
    m_prevQsosList->setFixedHeight(100);

    auto *rightPanel = new QWidget(this);
    rightPanel->setAutoFillBackground(true);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(6, 4, 6, 4);
    rightLayout->setSpacing(4);
    auto *lookupRow = new QHBoxLayout;
    lookupRow->setSpacing(6);
    lookupRow->addWidget(m_contactImage, 0, Qt::AlignTop);
    lookupRow->addWidget(m_lookupLabel, 1, Qt::AlignTop);

    rightLayout->addWidget(makeSectionHeader(tr("Callsign Lookup")));
    rightLayout->addWidget(makeDivider());
    rightLayout->addLayout(lookupRow);
    rightLayout->addWidget(m_prevQsosHeader);
    rightLayout->addWidget(makeDivider());
    rightLayout->addWidget(m_prevQsosList);
    rightLayout->addStretch();

    // Subtle visual separation between left and right columns
    auto *colSep = new QFrame(this);
    colSep->setFrameShape(QFrame::VLine);
    colSep->setFrameShadow(QFrame::Sunken);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(leftForm, 3);
    root->addWidget(colSep);
    root->addWidget(rightPanel, 2);

    clearForm();
    onModeChanged(m_mode->currentIndex());
}

// ---------------------------------------------------------------------------
// Public API — radio / digital listener integration
// ---------------------------------------------------------------------------

void QsoQuickEntryPanel::setRadioFreq(double freqMhz)
{
    setFreqSilently(freqMhz);
    const QString band = BandPlan::freqToBand(freqMhz);
    if (!band.isEmpty())
        setBandSilently(band);
}

void QsoQuickEntryPanel::setRadioMode(const QString &mode, const QString &submode)
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

void QsoQuickEntryPanel::setDxCall(const QString &call)
{
    m_callsign->setText(call.toUpper());
}

void QsoQuickEntryPanel::setDxGrid(const QString &grid)
{
    m_stationQso.gridsquare = grid.toUpper();
}

void QsoQuickEntryPanel::setLookupStatus(const QString &status)
{
    if (m_imageReply) {
        m_imageReply->abort();
        m_imageReply = nullptr;
    }
    m_fullPixmap = QPixmap();
    m_contactImage->clear();
    m_contactImage->setVisible(false);
    m_lookupLabel->setText(status);
}

void QsoQuickEntryPanel::setLookupResult(const CallsignLookupResult &result)
{
    // Abort any in-flight image fetch for the previous callsign
    if (m_imageReply) {
        m_imageReply->abort();
        m_imageReply = nullptr;
    }
    m_contactImage->clear();
    m_contactImage->setVisible(false);

    if (result.callsign.isEmpty()) {
        m_lookupLabel->setText(tr("No data found."));
        return;
    }

    if (!result.imageUrl.isEmpty()) {
        m_imageReply = m_nam->get(QNetworkRequest(QUrl(result.imageUrl)));
        connect(m_imageReply, &QNetworkReply::finished, this, [this, reply = m_imageReply]() {
            if (reply->error() == QNetworkReply::NoError) {
                QPixmap px;
                if (px.loadFromData(reply->readAll())) {
                    m_fullPixmap = px;
                    m_contactImage->setPixmap(
                        px.scaled(100, 130, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    m_contactImage->setVisible(true);
                }
            }
            reply->deleteLater();
            if (m_imageReply == reply)
                m_imageReply = nullptr;
        });
    }

    QStringList lines;
    if (!result.name.isEmpty())
        lines << result.name;

    QStringList loc;
    if (!result.qth.isEmpty())   loc << result.qth;
    if (!result.state.isEmpty()) loc << result.state;
    if (!result.country.isEmpty()) loc << result.country;
    if (!loc.isEmpty())
        lines << loc.join(QStringLiteral(", "));

    if (!result.gridsquare.isEmpty())
        lines << tr("Grid: %1").arg(result.gridsquare);

    QStringList dxInfo;
    if (result.dxcc > 0)    dxInfo << tr("DXCC %1").arg(result.dxcc);
    if (result.cqZone > 0)  dxInfo << tr("CQ %1").arg(result.cqZone);
    if (result.ituZone > 0) dxInfo << tr("ITU %1").arg(result.ituZone);
    if (!dxInfo.isEmpty())
        lines << dxInfo.join(QStringLiteral("  "));

    if (!result.licenseClass.isEmpty())
        lines << tr("Class: %1").arg(result.licenseClass);

    m_lookupLabel->setText(lines.join(QStringLiteral("\n")));

    // Store all mappable fields in the lookup layer for buildQso().
    // Station sources (WSJT-X etc.) take priority when merging.
    m_lookupQso = Qso{};
    m_lookupQso.name       = result.name;
    m_lookupQso.qth        = result.qth;
    m_lookupQso.state      = result.state;
    m_lookupQso.county     = result.county;
    m_lookupQso.country    = result.country;
    m_lookupQso.gridsquare = result.gridsquare.toUpper();
    m_lookupQso.cont       = result.cont;
    m_lookupQso.dxcc       = result.dxcc;
    m_lookupQso.cqZone     = result.cqZone;
    m_lookupQso.ituZone    = result.ituZone;
    m_lookupQso.lat        = result.lat;
    m_lookupQso.lon        = result.lon;
}

bool QsoQuickEntryPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_contactImage && event->type() == QEvent::MouseButtonPress
            && !m_fullPixmap.isNull()) {
        auto *dlg = new QDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowTitle(tr("Contact Photo"));

        auto *lbl = new QLabel(dlg);
        lbl->setPixmap(m_fullPixmap);
        lbl->setAlignment(Qt::AlignCenter);

        auto *scroll = new QScrollArea(dlg);
        scroll->setWidget(lbl);
        scroll->setAlignment(Qt::AlignCenter);
        scroll->setWidgetResizable(false);

        auto *layout = new QVBoxLayout(dlg);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(scroll);

        dlg->resize(qMin(m_fullPixmap.width()  + 4,  1200),
                    qMin(m_fullPixmap.height() + 40,  900));
        dlg->exec();
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void QsoQuickEntryPanel::setPreviousQsos(const QList<Qso> &qsos, int total)
{
    m_prevQsosList->clear();

    if (qsos.isEmpty()) {
        m_prevQsosList->addItem(tr("No previous QSOs found."));
        m_prevQsosHeader->setText(tr("Previous QSOs"));
        return;
    }

    m_prevQsosHeader->setText(tr("Previous QSOs (%1 total)").arg(total));
    for (const Qso &q : qsos) {
        const QString date = q.datetimeOn.toUTC().toString(QStringLiteral("yyyy-MM-dd"));
        m_prevQsosList->addItem(
            QStringLiteral("  %1   %2   %3").arg(date, q.band, q.mode));
    }
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void QsoQuickEntryPanel::clearLookupPanel()
{
    if (m_imageReply) {
        m_imageReply->abort();
        m_imageReply = nullptr;
    }
    m_fullPixmap = QPixmap();
    m_contactImage->clear();
    m_contactImage->setVisible(false);
    m_lookupLabel->setText(tr("Enter a callsign to look up station info."));
    m_prevQsosHeader->setText(tr("Previous QSOs"));
    m_prevQsosList->clear();
    m_prevQsosList->addItem(tr("Enter a callsign above."));
    m_lookupQso  = Qso{};
    m_stationQso = Qso{};
}

void QsoQuickEntryPanel::clearForm()
{
    clearLookupPanel();

    m_dateTime->setDateTime(QDateTime::currentDateTimeUtc());
    m_callsign->clear();
    m_rstSent->clear();
    m_rstRcvd->clear();
    m_comment->clear();

    if (m_mode->currentIndex() < 0)
        m_mode->setCurrentText(QStringLiteral("SSB"));

    updateRstDefaults();
    m_callsign->setFocus();
}

void QsoQuickEntryPanel::onNowClicked()
{
    m_dateTime->setDateTime(QDateTime::currentDateTimeUtc());
}

void QsoQuickEntryPanel::onModeChanged(int /*index*/)
{
    const QString mode = m_mode->currentText();
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

    if (m_suppressBandFreqSignals) return;
    updateRstDefaults();
}

void QsoQuickEntryPanel::onFreqChanged(double freqMhz)
{
    if (m_suppressBandFreqSignals || freqMhz == 0.0) return;
    const QString band = BandPlan::freqToBand(freqMhz);
    if (!band.isEmpty())
        setBandSilently(band);
}

void QsoQuickEntryPanel::onBandChanged(int index)
{
    if (m_suppressBandFreqSignals || index == 0) return;
    const double defFreq = BandPlan::bandDefaultFreq(m_band->currentText());
    if (defFreq > 0.0)
        setFreqSilently(defFreq);
}

void QsoQuickEntryPanel::onLogClicked()
{
    if (!validate()) return;

    Qso qso = buildQso();
    applyStationDefaults(qso);
    emit qsoReady(qso);

    const QString band    = m_band->currentText();
    const int     modeIdx = m_mode->currentIndex();
    const double  freq    = m_freq->value();

    clearForm();

    setBandSilently(band);
    setFreqSilently(freq);
    m_suppressBandFreqSignals = true;
    m_mode->setCurrentIndex(modeIdx);
    m_suppressBandFreqSignals = false;
    onModeChanged(modeIdx);

    m_callsign->setFocus();
    m_callsign->selectAll();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool QsoQuickEntryPanel::validate() const
{
    if (m_callsign->text().trimmed().isEmpty()) {
        m_callsign->setFocus();
        m_callsign->setStyleSheet(QStringLiteral("border: 1px solid red"));
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

void QsoQuickEntryPanel::enrichQso(Qso &qso) const
{
    // For each derived field: if the incoming qso already has a value, leave it
    // (the caller's source is authoritative). Otherwise fill from station layer
    // first, then lookup layer as the base.
    auto ms = [](QString &f, const QString &sta, const QString &lkp) {
        if (f.isEmpty()) f = !sta.isEmpty() ? sta : lkp;
    };
    auto mi = [](int &f, int sta, int lkp) {
        if (f == 0) f = sta > 0 ? sta : lkp;
    };

    ms(qso.gridsquare, m_stationQso.gridsquare, m_lookupQso.gridsquare);
    ms(qso.name,       m_stationQso.name,       m_lookupQso.name);
    ms(qso.qth,        m_stationQso.qth,        m_lookupQso.qth);
    ms(qso.state,      m_stationQso.state,      m_lookupQso.state);
    ms(qso.county,     m_stationQso.county,     m_lookupQso.county);
    ms(qso.country,    m_stationQso.country,    m_lookupQso.country);
    ms(qso.cont,       m_stationQso.cont,       m_lookupQso.cont);
    mi(qso.dxcc,       m_stationQso.dxcc,       m_lookupQso.dxcc);
    mi(qso.cqZone,     m_stationQso.cqZone,     m_lookupQso.cqZone);
    mi(qso.ituZone,    m_stationQso.ituZone,    m_lookupQso.ituZone);
    if (!qso.lat.has_value())
        qso.lat = m_stationQso.lat.has_value() ? m_stationQso.lat : m_lookupQso.lat;
    if (!qso.lon.has_value())
        qso.lon = m_stationQso.lon.has_value() ? m_stationQso.lon : m_lookupQso.lon;
}

Qso QsoQuickEntryPanel::buildQso() const
{
    Qso q;
    q.datetimeOn = m_dateTime->dateTime().toUTC();
    q.callsign   = m_callsign->text().toUpper().trimmed();
    q.band       = (m_band->currentIndex() > 0) ? m_band->currentText() : QString();
    q.freq       = m_freq->value();
    q.mode       = m_mode->currentText();
    q.submode    = m_submode->isEnabled() ? m_submode->currentText() : QString();
    q.rstSent    = m_rstSent->text().trimmed();
    q.rstRcvd    = m_rstRcvd->text().trimmed();
    q.comment    = m_comment->text().trimmed();
    enrichQso(q);
    return q;
}

void QsoQuickEntryPanel::applyStationDefaults(Qso &q) const
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
    const double pwr  = s.equipmentTxPwr();
    if (pwr > 0) q.txPwr = pwr;
    if (q.rig.isEmpty())     q.rig     = s.equipmentRig();
    if (q.antenna.isEmpty()) q.antenna = s.equipmentAntenna();
}

void QsoQuickEntryPanel::updateRstDefaults()
{
    const QString def = BandPlan::modeDefaultRst(m_mode->currentText());
    if (m_rstSent->text().isEmpty()) m_rstSent->setText(def);
    if (m_rstRcvd->text().isEmpty()) m_rstRcvd->setText(def);
}

void QsoQuickEntryPanel::setFreqSilently(double freqMhz)
{
    m_suppressBandFreqSignals = true;
    m_freq->setValue(freqMhz);
    m_suppressBandFreqSignals = false;
}

void QsoQuickEntryPanel::setBandSilently(const QString &band)
{
    m_suppressBandFreqSignals = true;
    const int idx = m_band->findText(band, Qt::MatchFixedString);
    if (idx >= 0) m_band->setCurrentIndex(idx);
    m_suppressBandFreqSignals = false;
}
