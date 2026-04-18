// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "QslUploadDialog.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTime>
#include <QVBoxLayout>

#include "database/DatabaseInterface.h"
#include "core/logbook/QsoFilter.h"
#include "qsl/QslService.h"

// Table column indices
static constexpr int COL_CHECK    = 0;
static constexpr int COL_DATE     = 1;
static constexpr int COL_TIME     = 2;
static constexpr int COL_CALL     = 3;
static constexpr int COL_BAND     = 4;
static constexpr int COL_MODE     = 5;
static constexpr int COL_FREQ     = 6;
static constexpr int COL_COUNT    = 7;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

QslUploadDialog::QslUploadDialog(const QList<QslService*> &services,
                                   DatabaseInterface        *db,
                                   QWidget                  *parent)
    : QDialog(parent)
    , m_db(db)
{
    setWindowTitle(tr("Upload QSOs to QSL Service"));
    setMinimumSize(700, 600);
    resize(780, 660);

    for (QslService *svc : services) {
        if (svc->isEnabled())
            m_services.append(svc);
    }

    // -----------------------------------------------------------------------
    // Row 1: service combo + Select Required button
    // -----------------------------------------------------------------------
    auto *serviceRow = new QHBoxLayout;
    m_serviceCombo = new QComboBox(this);
    for (QslService *svc : m_services)
        m_serviceCombo->addItem(svc->displayName());

    m_selectBtn = new QPushButton(tr("Select Required"), this);
    m_selectBtn->setEnabled(!m_services.isEmpty());

    serviceRow->addWidget(new QLabel(tr("Service:"), this));
    serviceRow->addWidget(m_serviceCombo, 1);
    serviceRow->addWidget(m_selectBtn);

    // -----------------------------------------------------------------------
    // QSO table — ~6 rows visible, scrollable
    // -----------------------------------------------------------------------
    m_table = new QTableWidget(0, COL_COUNT, this);
    m_table->setHorizontalHeaderLabels({
        QString(), tr("Date"), tr("Time (UTC)"), tr("Callsign"),
        tr("Band"), tr("Mode"), tr("Freq (MHz)")
    });

    // Checkbox column narrow; let Callsign and rest stretch
    m_table->horizontalHeader()->setSectionResizeMode(COL_CHECK, QHeaderView::Fixed);
    m_table->setColumnWidth(COL_CHECK, 28);
    m_table->horizontalHeader()->setSectionResizeMode(COL_DATE,  QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(COL_TIME,  QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(COL_CALL,  QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(COL_BAND,  QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(COL_MODE,  QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(COL_FREQ,  QHeaderView::ResizeToContents);

    m_table->verticalHeader()->hide();
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);

    // Fix height to ~6 rows + header
    const int rowH    = m_table->verticalHeader()->defaultSectionSize();
    const int headerH = m_table->horizontalHeader()->sizeHint().height();
    m_table->setMinimumHeight(headerH + rowH * 6);
    m_table->setMaximumHeight(headerH + rowH * 6 + 4);

    // -----------------------------------------------------------------------
    // Upload button (below table, full-width feel)
    // -----------------------------------------------------------------------
    m_uploadBtn = new QPushButton(tr("Upload"), this);
    m_uploadBtn->setEnabled(false);

    // -----------------------------------------------------------------------
    // Separator
    // -----------------------------------------------------------------------
    auto *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);

    // -----------------------------------------------------------------------
    // Log area
    // -----------------------------------------------------------------------
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_log->setPlaceholderText(tr("Upload log will appear here..."));

    // -----------------------------------------------------------------------
    // Status bar + Close button
    // -----------------------------------------------------------------------
    m_statusLabel = new QLabel(tr("Click \"Select Required\" to load unsent QSOs."), this);
    m_closeBtn    = new QPushButton(tr("Close"), this);

    auto *bottomRow = new QHBoxLayout;
    bottomRow->addWidget(m_statusLabel, 1);
    bottomRow->addWidget(m_closeBtn);

    // -----------------------------------------------------------------------
    // Main layout
    // -----------------------------------------------------------------------
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(serviceRow);
    mainLayout->addWidget(m_table);
    mainLayout->addWidget(m_uploadBtn);
    mainLayout->addWidget(sep);
    mainLayout->addWidget(m_log, 1);
    mainLayout->addLayout(bottomRow);

    // -----------------------------------------------------------------------
    // Connections
    // -----------------------------------------------------------------------
    connect(m_serviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QslUploadDialog::onServiceChanged);
    connect(m_selectBtn,  &QPushButton::clicked, this, &QslUploadDialog::onSelectRequired);
    connect(m_uploadBtn,  &QPushButton::clicked, this, &QslUploadDialog::onUploadClicked);
    connect(m_closeBtn,   &QPushButton::clicked, this, &QDialog::close);

    if (!m_services.isEmpty())
        onServiceChanged(0);

    if (m_services.isEmpty()) {
        m_statusLabel->setText(tr("No QSL services are enabled. Enable one in Settings → QSL Services."));
    }
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void QslUploadDialog::onServiceChanged(int /*index*/)
{
    if (m_services.isEmpty()) return;
    m_currentService = m_services.at(m_serviceCombo->currentIndex());

    // Clear table whenever service changes — user must re-query
    m_table->setRowCount(0);
    m_tableQsos.clear();
    updateUploadButton();
    m_statusLabel->setText(tr("Click \"Select Required\" to load unsent QSOs."));
}

void QslUploadDialog::onSelectRequired()
{
    if (!m_currentService || !m_db) return;

    // Build the appropriate unsent filter for this service
    QsoFilter filter;
    const QString name = m_currentService->displayName();
    if      (name == QLatin1String("LoTW"))    filter.lotwUnsent    = true;
    else if (name == QLatin1String("eQSL"))    filter.eqslUnsent    = true;
    else if (name == QLatin1String("QRZ"))     filter.qrzUnsent     = true;
    else if (name == QLatin1String("ClubLog")) filter.clublogUnsent = true;
    filter.ascending = false;   // newest first

    const auto result = m_db->fetchQsos(filter);
    if (!result) {
        m_statusLabel->setText(tr("Query failed: %1").arg(result.error()));
        return;
    }

    m_tableQsos = *result;
    m_table->setRowCount(0);

    for (const Qso &q : m_tableQsos) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);

        // Checkbox cell
        auto *chk = new QTableWidgetItem;
        chk->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        chk->setCheckState(Qt::Checked);
        chk->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, COL_CHECK, chk);

        auto makeItem = [](const QString &text) {
            auto *item = new QTableWidgetItem(text);
            item->setFlags(Qt::ItemIsEnabled);
            return item;
        };

        m_table->setItem(row, COL_DATE, makeItem(q.datetimeOn.toUTC().toString(QStringLiteral("yyyy-MM-dd"))));
        m_table->setItem(row, COL_TIME, makeItem(q.datetimeOn.toUTC().toString(QStringLiteral("HH:mm"))));
        m_table->setItem(row, COL_CALL, makeItem(q.callsign));
        m_table->setItem(row, COL_BAND, makeItem(q.band));
        m_table->setItem(row, COL_MODE, makeItem(q.submode.isEmpty() ? q.mode
                                                                      : QStringLiteral("%1/%2").arg(q.mode, q.submode)));
        m_table->setItem(row, COL_FREQ, makeItem(QString::number(q.freq, 'f', 3)));
    }

    updateUploadButton();

    if (m_tableQsos.isEmpty())
        m_statusLabel->setText(tr("No unsent QSOs for %1.").arg(name));
    else
        m_statusLabel->setText(tr("%1 unsent QSO(s) for %2 — deselect any to exclude.")
                                   .arg(m_tableQsos.size()).arg(name));
}

void QslUploadDialog::onUploadClicked()
{
    if (!m_currentService) return;

    // Collect only checked rows
    QList<Qso> toUpload;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (m_table->item(row, COL_CHECK)->checkState() == Qt::Checked)
            toUpload.append(m_tableQsos.at(row));
    }

    if (toUpload.isEmpty()) {
        m_statusLabel->setText(tr("No QSOs selected — check at least one row."));
        return;
    }

    m_log->clear();
    setRunning(true);

    appendLog(tr("Uploading %1 QSO(s) to %2...")
                  .arg(toUpload.size()).arg(m_currentService->displayName()));

    m_serviceConns << connect(m_currentService, &QslService::logMessage,
                              this, &QslUploadDialog::appendLog);
    m_serviceConns << connect(m_currentService, &QslService::uploadFinished,
                              this, &QslUploadDialog::onUploadFinished);

    m_currentService->startUpload(toUpload);
}

void QslUploadDialog::onUploadFinished(const QList<Qso> &updated, const QStringList &errors)
{
    disconnectService();
    setRunning(false);

    if (!errors.isEmpty())
        appendLog(tr("Messages: %1").arg(errors.join(QLatin1String("; "))));

    const QString summary = updated.isEmpty()
        ? tr("Upload complete — no QSOs sent.")
        : tr("Upload complete — %1 QSO(s) sent.").arg(updated.size());

    appendLog(summary);
    m_statusLabel->setText(summary);

    // Refresh the table to reflect the updated sent status
    if (!updated.isEmpty())
        onSelectRequired();

    emit uploadCompleted(updated, errors);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void QslUploadDialog::updateUploadButton()
{
    const bool hasRows = m_table->rowCount() > 0;
    m_uploadBtn->setEnabled(hasRows && !m_running);
    if (m_currentService)
        m_uploadBtn->setText(tr("Upload to %1").arg(m_currentService->displayName()));
    else
        m_uploadBtn->setText(tr("Upload"));
}

void QslUploadDialog::appendLog(const QString &msg)
{
    const QString ts = QTime::currentTime().toString(QStringLiteral("[HH:mm:ss] "));
    const QStringList lines = msg.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines)
        m_log->appendPlainText(ts + line.trimmed());
}

void QslUploadDialog::setRunning(bool running)
{
    m_running = running;
    m_uploadBtn->setEnabled(!running && m_table->rowCount() > 0);
    m_selectBtn->setEnabled(!running);
    m_serviceCombo->setEnabled(!running);
    m_table->setEnabled(!running);
    m_statusLabel->setText(running ? tr("Uploading...") : tr("Ready."));
}

void QslUploadDialog::disconnectService()
{
    for (auto &c : m_serviceConns)
        disconnect(c);
    m_serviceConns.clear();
}

void QslUploadDialog::closeEvent(QCloseEvent *event)
{
    if (m_running && m_currentService) {
        disconnectService();
        m_currentService->abort();
        m_running = false;
        appendLog(tr("Upload aborted."));
    }
    QDialog::closeEvent(event);
}
