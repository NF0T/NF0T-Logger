#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableView>
#include <QWidget>

#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QFileDialog>
#include <QKeySequence>
#include <QMenu>
#include <QProgressDialog>
#include <QShortcut>
#include <QSizePolicy>
#include <QTimer>

#include "app/settings/SecureSettings.h"
#include "app/settings/Settings.h"
#include "app/settings/SettingsDialog.h"
#include "wsjtx/WsjtxService.h"
#include "core/adif/AdifParser.h"
#include "core/adif/AdifWriter.h"
#include "core/logbook/QsoTableModel.h"
#include "core/Maidenhead.h"
#include "database/MariaDbBackend.h"
#include "database/SqliteBackend.h"
#include "qsl/ClubLogService.h"
#include "qsl/EqslService.h"
#include "qsl/LoTwService.h"
#include "qsl/QrzService.h"
#include "qsl/QslService.h"
#include "radio/DigitalListenerService.h"
#include "radio/HamlibBackend.h"
#include "radio/RadioBackend.h"
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
    setWindowIcon(QIcon(QStringLiteral(":/resources/icons/nf0t-logger.svg")));
    setMinimumSize(900, 600);
    resize(1200, 750);

    setupMenuBar();
    setupCentralWidget();
    setupStatusBar();

    // Restore window geometry before showing
    const QByteArray geo   = Settings::instance().mainWindowGeometry();
    const QByteArray state = Settings::instance().mainWindowState();
    if (!geo.isEmpty())   restoreGeometry(geo);
    if (!state.isEmpty()) restoreState(state);

    // Show credential-load status; enable QSL/radio menus once loaded
    connect(&SecureSettings::instance(), &SecureSettings::loaded, this, [this]() {
        showStatusMessage(tr("Credentials loaded."), 2000);
    });
    connect(&SecureSettings::instance(), &SecureSettings::error, this,
            [this](const QString &key, const QString &msg) {
        showStatusMessage(tr("Keychain error (%1): %2").arg(key, msg), 5000);
    });

    // First-run: require station callsign before opening the log
    if (Settings::instance().stationCallsign().isEmpty()) {
        auto *dlg = new SettingsDialog(this);
        dlg->showPage(SettingsDialog::Station);
        dlg->setWindowTitle(tr("Settings — please enter your station callsign"));
        dlg->exec();
    }

    openDefaultDatabase();

    // Radio backends — wire generically, then auto-connect if configured
    m_hamlibBackend = new HamlibBackend(this);
    m_tciBackend    = new TciBackend(this);
    m_radioBackends       = {m_hamlibBackend, m_tciBackend};
    m_radioConnectActions = {m_connectHamlibAction, m_connectTciAction};

    for (RadioBackend *b : m_radioBackends)
        wireRadioBackend(b);

    if (Settings::instance().hamlibEnabled())
        m_hamlibBackend->connectRadio();
    if (Settings::instance().tciEnabled())
        m_tciBackend->connectRadio();

    // QSL services — single list used by both dialogs
    m_lotwService    = new LoTwService(this);
    m_eqslService    = new EqslService(this);
    m_qrzService     = new QrzService(this);
    m_clublogService = new ClubLogService(this);
    m_qslServices    = {m_lotwService, m_eqslService, m_qrzService};

    // Digital listeners
    m_wsjtxService     = new WsjtxService(this);
    m_digitalListeners = {m_wsjtxService};

    for (DigitalListenerService *svc : m_digitalListeners)
        wireDigitalListener(svc);

    // Re-start / stop each listener when settings change
    connect(&Settings::instance(), &Settings::changed, this, [this]() {
        for (DigitalListenerService *svc : m_digitalListeners) {
            if (svc->isEnabled() && !svc->isRunning())
                svc->start();
            else if (!svc->isEnabled() && svc->isRunning())
                svc->stop();
        }
    });

}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Disconnect and shut down radio backends before Qt's child destruction
    // order can deliver signals to already-destroyed status bar widgets.
    for (RadioBackend *b : m_radioBackends) {
        disconnect(b, nullptr, this, nullptr);
        b->disconnectRadio();
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

void MainWindow::setupCentralWidget()
{
    m_splitter = new QSplitter(Qt::Vertical, this);

    // Log table (top pane)
    m_logModel = new QsoTableModel(this);
    m_logView  = new QTableView(m_splitter);

    // Install grouped QSL header before setting the model
    auto *qslHeader = new QslGroupHeaderView(Qt::Horizontal, m_logView);
    qslHeader->setStretchLastSection(false);
    qslHeader->setHighlightSections(false);
    m_logView->setHorizontalHeader(qslHeader);

    m_logView->setModel(m_logModel);
    qslHeader->setSectionResizeMode(QsoTableModel::ColName, QHeaderView::Stretch);
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

    // Escape anywhere in the main window resets the entry panel
    auto *escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escShortcut, &QShortcut::activated, m_entryPanel, &QsoEntryPanel::clearForm);

    m_splitter->addWidget(m_logView);
    m_splitter->addWidget(m_entryPanel);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);

    setCentralWidget(m_splitter);
}

static const char *kIndicatorIdle  =
    "QLabel { color: palette(window-text); border: 1px solid palette(mid);"
    " border-radius: 4px; padding: 2px 8px; }";
static const char *kIndicatorGreen =
    "QLabel { background: #2ea44f; color: white;"
    " border-radius: 4px; padding: 2px 8px; }";
static const char *kIndicatorRed   =
    "QLabel { background: #cf222e; color: white;"
    " border-radius: 4px; padding: 2px 8px; }";

void MainWindow::setIndicatorState(QLabel *indicator, IndicatorState state)
{
    if (!indicator) return;
    switch (state) {
    case IndicatorState::Idle:      indicator->setStyleSheet(kIndicatorIdle);  break;
    case IndicatorState::Connected: indicator->setStyleSheet(kIndicatorGreen); break;
    case IndicatorState::Fault:     indicator->setStyleSheet(kIndicatorRed);   break;
    }
}

void MainWindow::showStatusMessage(const QString &msg, int ms)
{
    m_statusMsgLabel->setText(msg);
    if (ms > 0)
        QTimer::singleShot(ms, this, [this]() { m_statusMsgLabel->clear(); });
}

void MainWindow::setupStatusBar()
{
    auto makeIndicator = [this](const QString &label) {
        auto *w = new QLabel(label, this);
        w->setStyleSheet(kIndicatorIdle);
        w->setContentsMargins(2, 0, 2, 0);
        return w;
    };

    m_hamlibIndicator = makeIndicator(tr("Hamlib"));
    m_tciIndicator    = makeIndicator(tr("TCI"));
    m_wsjtxIndicator  = makeIndicator(tr("WSJT-X"));

    m_statusMsgLabel  = new QLabel(this);
    m_statusMsgLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_qsoCountLabel   = new QLabel(tr("QSOs: 0"), this);

    statusBar()->addWidget(m_hamlibIndicator);
    statusBar()->addWidget(m_tciIndicator);
    statusBar()->addWidget(m_wsjtxIndicator);
    statusBar()->addWidget(m_statusMsgLabel);
    statusBar()->addPermanentWidget(m_qsoCountLabel);
}

// ---------------------------------------------------------------------------
// Database
// ---------------------------------------------------------------------------

void MainWindow::openDefaultDatabase()
{
    const Settings &cfg = Settings::instance();
    const QString backendKey = cfg.dbBackend();   // "sqlite" | "mariadb"

    std::unique_ptr<DatabaseInterface> backend;
    QVariantMap config;

    if (backendKey == QLatin1String("mariadb")) {
        config = {
            {"host",     cfg.dbMariadbHost()},
            {"port",     cfg.dbMariadbPort()},
            {"name",     cfg.dbMariadbDatabase()},
            {"user",     cfg.dbMariadbUsername()},
            {"password", cfg.dbMariadbPassword()},
        };
        backend = std::make_unique<MariaDbBackend>();
    } else {
        const QString dataDir =
            QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        QDir().mkpath(dataDir);
        config  = {{"path", dataDir + "/log.db"}};
        backend = std::make_unique<SqliteBackend>();
    }

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
    const QString label = backendKey == QLatin1String("mariadb")
        ? tr("MariaDB (%1)").arg(cfg.dbMariadbHost())
        : config["path"].toString();
    showStatusMessage(tr("Database opened: %1").arg(label), 4000);
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

// Derive missing lat/lon from gridsquare and compute distance from my station.
static void enrichQso(Qso &qso)
{
    // Derive DX lat/lon from grid square if not explicitly provided
    if (!qso.gridsquare.isEmpty() && !qso.lat.has_value()) {
        double lat, lon;
        if (Maidenhead::toLatLon(qso.gridsquare, lat, lon)) {
            qso.lat = lat;
            qso.lon = lon;
        }
    }

    if (qso.distance.has_value()) return;

    // Need a DX position to measure from
    if (!qso.lat.has_value()) return;

    // Prefer my grid square for my position; fall back to stored lat/lon
    const QString myGrid = Settings::instance().stationGridsquare();
    if (!myGrid.isEmpty()) {
        if (auto d = Maidenhead::distanceKm(myGrid, *qso.lat, *qso.lon))
            qso.distance = *d;
    } else {
        const auto mLat = Settings::instance().stationLat();
        const auto mLon = Settings::instance().stationLon();
        if (mLat.has_value() && mLon.has_value())
            qso.distance = Maidenhead::distanceKm(*mLat, *mLon, *qso.lat, *qso.lon);
    }
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
        enrichQso(qso);
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
            tr("Hamlib is not enabled. Enable it in Settings \u2192 Radio."));
        return;
    }
    m_hamlibBackend->connectRadio();
}

void MainWindow::onConnectTci()
{
    if (!Settings::instance().tciEnabled()) {
        QMessageBox::information(this, tr("TCI"),
            tr("TCI is not enabled. Enable it in Settings \u2192 Radio."));
        return;
    }
    m_tciBackend->connectRadio();
}

void MainWindow::onDisconnectRadio()
{
    for (RadioBackend *b : m_radioBackends)
        b->disconnectRadio();
}

// ---------------------------------------------------------------------------
// Generic backend wiring helpers
// ---------------------------------------------------------------------------

bool MainWindow::anyRadioConnected() const
{
    for (const RadioBackend *b : m_radioBackends)
        if (b->isConnected()) return true;
    return false;
}

void MainWindow::wireRadioBackend(RadioBackend *backend)
{
    QLabel *indicator = (backend == m_hamlibBackend) ? m_hamlibIndicator : m_tciIndicator;

    connect(backend, &RadioBackend::freqChanged,
            m_entryPanel, &QsoEntryPanel::setRadioFreq);
    connect(backend, &RadioBackend::modeChanged,
            m_entryPanel, &QsoEntryPanel::setRadioMode);

    connect(backend, &RadioBackend::connected, this, [this, backend, indicator]() {
        setIndicatorState(indicator, IndicatorState::Connected);
        for (QAction *act : m_radioConnectActions)
            act->setEnabled(false);
        m_disconnectRadioAction->setEnabled(true);
        showStatusMessage(
            tr("%1 connected.").arg(backend->displayName()), 3000);
    });

    connect(backend, &RadioBackend::disconnected, this, [this, indicator]() {
        setIndicatorState(indicator, IndicatorState::Idle);
        if (!anyRadioConnected()) {
            for (QAction *act : m_radioConnectActions)
                act->setEnabled(true);
            m_disconnectRadioAction->setEnabled(false);
        }
        showStatusMessage(tr("Radio disconnected."), 3000);
    });

    connect(backend, &RadioBackend::error, this, [this, backend, indicator](const QString &msg) {
        setIndicatorState(indicator, IndicatorState::Fault);
        showStatusMessage(
            tr("%1 error: %2").arg(backend->displayName(), msg), 6000);
    });
}

void MainWindow::wireDigitalListener(DigitalListenerService *svc)
{
    QLabel *indicator = (svc == m_wsjtxService) ? m_wsjtxIndicator : nullptr;

    connect(svc, &DigitalListenerService::started, this, [this, indicator]() {
        setIndicatorState(indicator, IndicatorState::Connected);
    });
    connect(svc, &DigitalListenerService::stopped, this, [this, indicator]() {
        setIndicatorState(indicator, IndicatorState::Idle);
    });

    connect(svc, &DigitalListenerService::radioStatusChanged,
            this, [this](quint64 dialFreqHz, const QString &mode, const QString &submode) {
        if (anyRadioConnected()) return;
        if (dialFreqHz > 0)
            m_entryPanel->setRadioFreq(static_cast<double>(dialFreqHz) / 1e6);
        if (!mode.isEmpty())
            m_entryPanel->setRadioMode(mode, submode);
    });

    connect(svc, &DigitalListenerService::stationSelected,
            this, [this](const QString &dxCall, const QString &dxGrid) {
        if (dxCall.isEmpty()) {
            m_entryPanel->clearForm();
        } else {
            m_entryPanel->setDxCall(dxCall);
            if (!dxGrid.isEmpty())
                m_entryPanel->setDxGrid(dxGrid);
        }
    });

    connect(svc, &DigitalListenerService::cleared,
            m_entryPanel, &QsoEntryPanel::clearForm);

    connect(svc, &DigitalListenerService::qsoLogged,
            this, [this](const Qso &qso) {
        if (!Settings::instance().wsjtxAutoLog()) return;
        onQsoReady(qso);
        m_entryPanel->clearForm();
    });

    connect(svc, &DigitalListenerService::logMessage,
            this, [this](const QString &msg) {
        showStatusMessage(msg, 3000);
    });

    if (svc->isEnabled())
        svc->start();
}

// ---------------------------------------------------------------------------
// QSL slots
// ---------------------------------------------------------------------------

void MainWindow::onQslDownload()
{
    if (!m_db) return;

    const QList<Qso> localQsos = m_db->fetchQsos().value_or(QList<Qso>{});
    QslDownloadDialog dlg(m_qslServices, localQsos, this);
    connect(&dlg, &QslDownloadDialog::downloadCompleted, this,
            [this](const QList<Qso> &confirmed, const QStringList &errors) {
        applyDownloadedConfirmations(confirmed, errors);
    });
    dlg.exec();
}

void MainWindow::onQslUpload()
{
    if (!m_db) return;

    QslUploadDialog dlg(m_qslServices, m_db.get(), this);
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
    showStatusMessage(tr("QSL upload: %1 QSO(s) marked sent.").arg(updated.size()), 4000);
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
    showStatusMessage(tr("QSL download: %1 QSO(s) updated.").arg(matched.size()), 4000);
}

void MainWindow::onQsoReady(const Qso &qso)
{
    if (!m_db) return;

    Qso inserted = qso;
    enrichQso(inserted);
    if (auto r = m_db->insertQso(inserted); !r) {
        showStatusMessage(
            tr("Failed to log contact: %1").arg(r.error()), 5000);
        return;
    }

    m_logModel->prependQso(inserted);
    updateQsoCount();
    showStatusMessage(
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
    showStatusMessage(tr("QSO updated: %1").arg(updated.callsign), 3000);
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
    showStatusMessage(tr("QSO deleted: %1").arg(q.callsign), 3000);
}
