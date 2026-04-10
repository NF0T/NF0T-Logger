#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableView>
#include <QToolBar>
#include <QWidget>

#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QFileDialog>
#include <QKeySequence>
#include <QMenu>
#include <QProgressDialog>
#include <QShortcut>

#include "app/settings/SecureSettings.h"
#include "app/settings/Settings.h"
#include "app/settings/SettingsDialog.h"
#include "core/adif/AdifParser.h"
#include "core/adif/AdifWriter.h"
#include "core/logbook/QsoTableModel.h"
#include "database/SqliteBackend.h"
#include "qsl/ClubLogService.h"
#include "qsl/EqslService.h"
#include "qsl/LoTwService.h"
#include "qsl/QrzService.h"
#include "radio/HamlibBackend.h"
#include "radio/TciBackend.h"
#include "ui/entrypanel/QsoEntryPanel.h"
#include "ui/QsoEditDialog.h"
#include "ui/QslColumns.h"
#include "ui/QslDownloadDialog.h"
#include "ui/QslUploadDialog.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("NF0T Logger");
    setMinimumSize(900, 600);
    resize(1200, 750);

    setupMenuBar();
    setupToolBar();
    setupCentralWidget();
    setupStatusBar();

    // Restore window geometry before showing
    const QByteArray geo   = Settings::instance().mainWindowGeometry();
    const QByteArray state = Settings::instance().mainWindowState();
    if (!geo.isEmpty())   restoreGeometry(geo);
    if (!state.isEmpty()) restoreState(state);

    // Show credential-load status; enable QSL/radio menus once loaded
    connect(&SecureSettings::instance(), &SecureSettings::loaded, this, [this]() {
        statusBar()->showMessage(tr("Credentials loaded."), 2000);
    });
    connect(&SecureSettings::instance(), &SecureSettings::error, this,
            [this](const QString &key, const QString &msg) {
        statusBar()->showMessage(tr("Keychain error (%1): %2").arg(key, msg), 5000);
    });

    // First-run: require station callsign before opening the log
    if (Settings::instance().stationCallsign().isEmpty()) {
        auto *dlg = new SettingsDialog(this);
        dlg->showPage(SettingsDialog::Station);
        dlg->setWindowTitle(tr("Settings — please enter your station callsign"));
        dlg->exec();
    }

    openDefaultDatabase();

    // Create the Hamlib backend (does not connect — user initiates via menu)
    m_hamlibBackend = new HamlibBackend(this);
    connect(m_hamlibBackend, &HamlibBackend::freqChanged,
            m_entryPanel,    &QsoEntryPanel::setRadioFreq);
    connect(m_hamlibBackend, &HamlibBackend::modeChanged,
            m_entryPanel,    &QsoEntryPanel::setRadioMode);
    connect(m_hamlibBackend, &HamlibBackend::connected, this, [this]() {
        m_radioStatusLabel->setText(tr("Radio: Connected (Hamlib)"));
        m_connectHamlibAction->setEnabled(false);
        m_disconnectRadioAction->setEnabled(true);
        statusBar()->showMessage(tr("Hamlib connected."), 3000);
    });
    connect(m_hamlibBackend, &HamlibBackend::disconnected, this, [this]() {
        m_radioStatusLabel->setText(tr("Radio: Not connected"));
        m_connectHamlibAction->setEnabled(true);
        m_disconnectRadioAction->setEnabled(false);
        statusBar()->showMessage(tr("Radio disconnected."), 3000);
    });
    connect(m_hamlibBackend, &HamlibBackend::error, this, [this](const QString &msg) {
        statusBar()->showMessage(tr("Radio error: %1").arg(msg), 6000);
    });

    // Auto-connect on startup if Hamlib was previously enabled
    if (Settings::instance().hamlibEnabled())
        m_hamlibBackend->connectRig();

    // TCI backend
    m_tciBackend = new TciBackend(this);
    connect(m_tciBackend, &TciBackend::freqChanged,
            m_entryPanel, &QsoEntryPanel::setRadioFreq);
    connect(m_tciBackend, &TciBackend::modeChanged,
            m_entryPanel, &QsoEntryPanel::setRadioMode);
    connect(m_tciBackend, &TciBackend::connected, this, [this]() {
        m_radioStatusLabel->setText(tr("Radio: Connected (TCI)"));
        m_connectTciAction->setEnabled(false);
        m_connectHamlibAction->setEnabled(false);
        m_disconnectRadioAction->setEnabled(true);
        statusBar()->showMessage(tr("TCI connected."), 3000);
    });
    connect(m_tciBackend, &TciBackend::disconnected, this, [this]() {
        m_radioStatusLabel->setText(tr("Radio: Not connected"));
        m_connectTciAction->setEnabled(true);
        m_connectHamlibAction->setEnabled(true);
        m_disconnectRadioAction->setEnabled(false);
        statusBar()->showMessage(tr("TCI disconnected."), 3000);
    });
    connect(m_tciBackend, &TciBackend::error, this, [this](const QString &msg) {
        statusBar()->showMessage(tr("TCI error: %1").arg(msg), 6000);
    });

    // Auto-connect on startup if TCI was previously enabled
    if (Settings::instance().tciEnabled())
        m_tciBackend->connectTci();

    // QSL services — owned here; dialogs borrow pointers at open time
    m_lotwService    = new LoTwService(this);
    m_eqslService    = new EqslService(this);
    m_qrzService     = new QrzService(this);
    m_clublogService = new ClubLogService(this);
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Disconnect and shut down radio backends before Qt's child destruction
    // order can deliver signals to already-destroyed status bar widgets.
    if (m_tciBackend) {
        disconnect(m_tciBackend, nullptr, this, nullptr);
        m_tciBackend->disconnectTci();
    }
    if (m_hamlibBackend) {
        disconnect(m_hamlibBackend, nullptr, this, nullptr);
        m_hamlibBackend->disconnectRig();
    }

    Settings::instance().setMainWindowGeometry(saveGeometry());
    Settings::instance().setMainWindowState(saveState());
    if (m_splitter)
        Settings::instance().setSplitterState(m_splitter->saveState());
    QMainWindow::closeEvent(event);
}

// ---------------------------------------------------------------------------
// Setup helpers
// ---------------------------------------------------------------------------

void MainWindow::setupMenuBar()
{
    // --- File ---
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

    m_newLogAction = new QAction(tr("&New Log..."), this);
    m_newLogAction->setShortcut(QKeySequence::New);
    connect(m_newLogAction, &QAction::triggered, this, &MainWindow::onNewLog);
    fileMenu->addAction(m_newLogAction);

    fileMenu->addSeparator();

    m_importAdifAction = new QAction(tr("&Import ADIF..."), this);
    m_importAdifAction->setShortcut(tr("Ctrl+I"));
    connect(m_importAdifAction, &QAction::triggered, this, &MainWindow::onImportAdif);
    fileMenu->addAction(m_importAdifAction);

    m_exportAdifAction = new QAction(tr("&Export ADIF..."), this);
    m_exportAdifAction->setShortcut(tr("Ctrl+E"));
    connect(m_exportAdifAction, &QAction::triggered, this, &MainWindow::onExportAdif);
    fileMenu->addAction(m_exportAdifAction);

    fileMenu->addSeparator();

    m_settingsAction = new QAction(tr("&Settings..."), this);
    m_settingsAction->setShortcut(QKeySequence::Preferences);
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::onSettingsDialog);
    fileMenu->addAction(m_settingsAction);

    fileMenu->addSeparator();

    m_exitAction = new QAction(tr("E&xit"), this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, qApp, &QApplication::quit);
    fileMenu->addAction(m_exitAction);

    // --- Radio ---
    QMenu *radioMenu = menuBar()->addMenu(tr("&Radio"));

    m_connectHamlibAction = new QAction(tr("Connect &Hamlib..."), this);
    connect(m_connectHamlibAction, &QAction::triggered, this, &MainWindow::onConnectHamlib);
    radioMenu->addAction(m_connectHamlibAction);

    m_connectTciAction = new QAction(tr("Connect &TCI..."), this);
    connect(m_connectTciAction, &QAction::triggered, this, &MainWindow::onConnectTci);
    radioMenu->addAction(m_connectTciAction);

    radioMenu->addSeparator();

    m_disconnectRadioAction = new QAction(tr("&Disconnect"), this);
    m_disconnectRadioAction->setEnabled(false);
    connect(m_disconnectRadioAction, &QAction::triggered, this, &MainWindow::onDisconnectRadio);
    radioMenu->addAction(m_disconnectRadioAction);

    // --- QSL ---
    QMenu *qslMenu = menuBar()->addMenu(tr("&QSL"));

    auto *qslDownloadAction = new QAction(tr("&Download QSL..."), this);
    qslDownloadAction->setShortcut(QKeySequence(tr("Ctrl+Shift+D")));
    connect(qslDownloadAction, &QAction::triggered, this, &MainWindow::onQslDownload);

    auto *qslUploadAction = new QAction(tr("&Upload QSL..."), this);
    qslUploadAction->setShortcut(QKeySequence(tr("Ctrl+Shift+U")));
    connect(qslUploadAction, &QAction::triggered, this, &MainWindow::onQslUpload);

    qslMenu->addAction(qslDownloadAction);
    qslMenu->addAction(qslUploadAction);

    // --- Help ---
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    m_aboutAction = new QAction(tr("&About NF0T Logger"), this);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
    helpMenu->addAction(m_aboutAction);
}

void MainWindow::setupToolBar()
{
    QToolBar *toolBar = addToolBar(tr("Main Toolbar"));
    toolBar->setObjectName(QStringLiteral("mainToolBar"));
    toolBar->setMovable(false);
    toolBar->addAction(m_newLogAction);
    toolBar->addSeparator();
    toolBar->addAction(m_importAdifAction);
    toolBar->addAction(m_exportAdifAction);
}

void MainWindow::setupCentralWidget()
{
    m_splitter = new QSplitter(Qt::Vertical, this);

    // Log table (top pane)
    m_logModel = new QsoTableModel(this);
    m_logView  = new QTableView(m_splitter);

    // Install grouped QSL header before setting the model
    auto *qslHeader = new QslGroupHeaderView(Qt::Horizontal, m_logView);
    qslHeader->setStretchLastSection(true);
    qslHeader->setHighlightSections(false);
    m_logView->setHorizontalHeader(qslHeader);

    m_logView->setModel(m_logModel);
    m_logView->setAlternatingRowColors(true);
    m_logView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_logView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_logView->setSortingEnabled(false);
    m_logView->verticalHeader()->hide();
    m_logView->verticalHeader()->setDefaultSectionSize(22);

    // Bubble delegate + narrow widths for QSL sub-columns
    auto *bubbleDelegate = new QslBubbleDelegate(m_logView);
    for (int c = QsoTableModel::ColQslFirst; c < QsoTableModel::ColCount; ++c) {
        m_logView->setItemDelegateForColumn(c, bubbleDelegate);
        m_logView->setColumnWidth(c, 24);
    }

    // Double-click to edit
    connect(m_logView, &QTableView::doubleClicked,
            this, &MainWindow::onEditQso);

    // Right-click context menu
    m_logView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_logView, &QTableView::customContextMenuRequested,
            this, [this](const QPoint &pos) {
        const QModelIndex idx = m_logView->indexAt(pos);
        if (!idx.isValid()) return;
        m_logView->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);

        QMenu menu(this);
        QAction *editAct   = menu.addAction(tr("Edit QSO…"));
        QAction *deleteAct = menu.addAction(tr("Delete QSO"));
        const QAction *chosen = menu.exec(m_logView->viewport()->mapToGlobal(pos));
        if (chosen == editAct)
            onEditQso(idx);
        else if (chosen == deleteAct)
            onDeleteSelectedQso();
    });

    // Delete key shortcut
    auto *deleteShortcut = new QShortcut(QKeySequence::Delete, m_logView);
    connect(deleteShortcut, &QShortcut::activated, this, &MainWindow::onDeleteSelectedQso);

    // Bottom pane — QSO entry panel
    m_entryPanel = new QsoEntryPanel(m_splitter);
    connect(m_entryPanel, &QsoEntryPanel::qsoReady, this, &MainWindow::onQsoReady);

    m_splitter->addWidget(m_logView);
    m_splitter->addWidget(m_entryPanel);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);

    setCentralWidget(m_splitter);
}

void MainWindow::setupStatusBar()
{
    m_radioStatusLabel = new QLabel(tr("Radio: Not connected"), this);
    m_qsoCountLabel    = new QLabel(tr("QSOs: 0"), this);

    statusBar()->addWidget(m_radioStatusLabel);
    statusBar()->addPermanentWidget(m_qsoCountLabel);
}

// ---------------------------------------------------------------------------
// Database
// ---------------------------------------------------------------------------

void MainWindow::openDefaultDatabase()
{
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataDir);
    const QString dbPath = dataDir + "/log.db";

    auto backend = std::make_unique<SqliteBackend>();
    const QVariantMap config = {{"path", dbPath}};

    if (auto r = backend->open(config); !r) {
        QMessageBox::critical(this, tr("Database Error"),
            tr("Could not open database:\n%1").arg(r.error()));
        return;
    }

    if (auto r = backend->initSchema(); !r) {
        QMessageBox::critical(this, tr("Database Error"),
            tr("Schema initialisation failed:\n%1").arg(r.error()));
        return;
    }

    m_db = std::move(backend);
    statusBar()->showMessage(tr("Database opened: %1").arg(dbPath), 4000);
    reloadLog();
}

void MainWindow::reloadLog()
{
    if (!m_db) return;
    if (auto r = m_db->fetchQsos())
        m_logModel->setQsos(*r);
    updateQsoCount();
}

void MainWindow::updateQsoCount()
{
    if (!m_db) return;
    const int n = m_db->qsoCount().value_or(0);
    m_qsoCountLabel->setText(tr("QSOs: %1").arg(n));
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void MainWindow::onNewLog()
{
    QMessageBox::information(this, tr("New Log"), tr("New log functionality coming soon."));
}

void MainWindow::onImportAdif()
{
    if (!m_db) return;

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import ADIF"), QString(),
        tr("ADIF Files (*.adi *.adif);;All Files (*)"));
    if (path.isEmpty()) return;

    QProgressDialog progress(tr("Parsing ADIF file…"), tr("Cancel"), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(300);
    progress.setValue(0);
    qApp->processEvents();

    const AdifParser::Result parsed = AdifParser::parseFile(path);

    int imported = 0, duplicates = 0, errors = 0;
    QStringList errorDetails = parsed.warnings;

    progress.setMaximum(parsed.qsos.size());
    for (int i = 0; i < parsed.qsos.size(); ++i) {
        if (progress.wasCanceled()) break;
        progress.setValue(i);

        Qso qso = parsed.qsos.at(i);
        if (auto r = m_db->insertQso(qso); !r) {
            const QString &err = r.error();
            // Unique constraint violations are expected for duplicates
            if (err.contains("UNIQUE", Qt::CaseInsensitive) ||
                err.contains("Duplicate", Qt::CaseInsensitive)) {
                ++duplicates;
            } else {
                ++errors;
                errorDetails << QString("Insert failed for %1: %2").arg(qso.callsign, err);
            }
        } else {
            ++imported;
        }
    }
    progress.setValue(parsed.qsos.size());

    reloadLog();

    QString summary = tr("Import complete.\n\nImported: %1\nDuplicates skipped: %2\n"
                         "Parse errors: %3\nDB errors: %4")
                          .arg(imported).arg(duplicates)
                          .arg(parsed.skipped).arg(errors);

    if (errorDetails.isEmpty()) {
        QMessageBox::information(this, tr("ADIF Import"), summary);
    } else {
        QMessageBox *box = new QMessageBox(QMessageBox::Warning, tr("ADIF Import"),
                                           summary, QMessageBox::Ok, this);
        box->setDetailedText(errorDetails.join('\n'));
        box->exec();
    }
}

void MainWindow::onExportAdif()
{
    if (!m_db) return;

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export ADIF"), QString(),
        tr("ADIF Files (*.adi);;All Files (*)"));
    if (path.isEmpty()) return;

    const QList<Qso> qsos = m_db->fetchQsos().value_or(QList<Qso>{});
    if (qsos.isEmpty()) {
        QMessageBox::information(this, tr("ADIF Export"), tr("No contacts to export."));
        return;
    }

    AdifWriter::Options opts;
    if (!AdifWriter::writeFile(path, qsos, opts)) {
        QMessageBox::critical(this, tr("ADIF Export"),
                              tr("Failed to write file:\n%1").arg(path));
        return;
    }

    QMessageBox::information(this, tr("ADIF Export"),
        tr("Exported %1 contact(s) to:\n%2").arg(qsos.size()).arg(path));
}

void MainWindow::onSettingsDialog()
{
    SettingsDialog dlg(this);
    dlg.exec();
    reloadLog();       // picks up unit changes, callsign changes, etc.
    updateQsoCount();
}

void MainWindow::onAbout()
{
    QMessageBox::about(this,
        tr("About NF0T Logger"),
        tr("<b>NF0T Logger</b> v0.1.0<br>"
           "Amateur Radio Contact Logger<br><br>"
           "Built with C++ and Qt6.")
    );
}

void MainWindow::onConnectHamlib()
{
    if (!Settings::instance().hamlibEnabled()) {
        QMessageBox::information(this, tr("Hamlib"),
            tr("Hamlib is not enabled. Enable it in Settings → Radio."));
        return;
    }
    m_hamlibBackend->connectRig();
}

void MainWindow::onConnectTci()
{
    if (!Settings::instance().tciEnabled()) {
        QMessageBox::information(this, tr("TCI"),
            tr("TCI is not enabled. Enable it in Settings → Radio."));
        return;
    }
    m_tciBackend->connectTci();
}

void MainWindow::onDisconnectRadio()
{
    m_hamlibBackend->disconnectRig();
    m_tciBackend->disconnectTci();
}

// ---------------------------------------------------------------------------
// QSL slots
// ---------------------------------------------------------------------------

void MainWindow::onQslDownload()
{
    if (!m_db) return;

    const QList<QslService*> services = {
        m_lotwService, m_eqslService, m_qrzService, m_clublogService
    };
    const QList<Qso> localQsos = m_db->fetchQsos().value_or(QList<Qso>{});

    QslDownloadDialog dlg(services, localQsos, this);
    connect(&dlg, &QslDownloadDialog::downloadCompleted, this,
            [this](const QList<Qso> &confirmed, const QStringList &errors) {
        applyDownloadedConfirmations(confirmed, errors);
    });
    dlg.exec();
}

void MainWindow::onQslUpload()
{
    if (!m_db) return;

    const QList<QslService*> services = {
        m_lotwService, m_eqslService, m_qrzService, m_clublogService
    };
    const QList<Qso> allQsos = m_db->fetchQsos().value_or(QList<Qso>{});

    QslUploadDialog dlg(services, allQsos, this);
    connect(&dlg, &QslUploadDialog::uploadCompleted, this,
            [this](const QList<Qso> &updated, const QStringList &) {
        applyUploadedQsos(updated, {});
    });
    dlg.exec();
}

void MainWindow::applyUploadedQsos(const QList<Qso> &updated, const QStringList &/*errors*/)
{
    if (updated.isEmpty() || !m_db) return;
    for (const Qso &q : updated)
        std::ignore = m_db->updateQso(q);
    reloadLog();
    statusBar()->showMessage(tr("QSL upload: %1 QSO(s) marked sent.").arg(updated.size()), 4000);
}

void MainWindow::applyDownloadedConfirmations(const QList<Qso> &matched,
                                               const QStringList &/*errors*/)
{
    // The dialog has already done matching and per-record logging.
    // matched contains local Qso objects with rcvd fields updated — write directly.
    if (matched.isEmpty() || !m_db) return;

    for (const Qso &q : matched)
        std::ignore = m_db->updateQso(q);

    reloadLog();
    statusBar()->showMessage(tr("QSL download: %1 QSO(s) updated.").arg(matched.size()), 4000);
}

void MainWindow::onQsoReady(const Qso &qso)
{
    if (!m_db) return;

    Qso inserted = qso;
    if (auto r = m_db->insertQso(inserted); !r) {
        statusBar()->showMessage(
            tr("Failed to log contact: %1").arg(r.error()), 5000);
        return;
    }

    m_logModel->prependQso(inserted);
    updateQsoCount();
    statusBar()->showMessage(
        tr("Logged: %1").arg(inserted.callsign), 3000);
}

void MainWindow::onEditQso(const QModelIndex &index)
{
    if (!index.isValid() || !m_db) return;

    const int row = index.row();
    const Qso original = m_logModel->qsoAt(row);

    QsoEditDialog dlg(original, this);
    if (dlg.exec() != QDialog::Accepted) return;

    Qso updated = dlg.qso();
    if (auto r = m_db->updateQso(updated); !r) {
        QMessageBox::warning(this, tr("Edit Failed"),
            tr("Could not save changes:\n%1").arg(r.error()));
        return;
    }

    m_logModel->updateQso(row, updated);
    statusBar()->showMessage(tr("QSO updated: %1").arg(updated.callsign), 3000);
}

void MainWindow::onDeleteSelectedQso()
{
    if (!m_db) return;

    const QModelIndex idx = m_logView->currentIndex();
    if (!idx.isValid()) return;

    const int row = idx.row();
    const Qso &q = m_logModel->qsoAt(row);

    const auto reply = QMessageBox::question(
        this, tr("Delete QSO"),
        tr("Delete QSO with %1 on %2?\nThis cannot be undone.")
            .arg(q.callsign, q.datetimeOn.toUTC().toString("yyyy-MM-dd HH:mm")),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    if (auto r = m_db->deleteQso(q.id); !r) {
        QMessageBox::warning(this, tr("Delete Failed"),
            tr("Could not delete QSO:\n%1").arg(r.error()));
        return;
    }

    m_logModel->removeQso(row);
    updateQsoCount();
    statusBar()->showMessage(tr("QSO deleted: %1").arg(q.callsign), 3000);
}
