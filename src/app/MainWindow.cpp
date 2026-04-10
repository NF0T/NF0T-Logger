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
#include <QFileDialog>
#include <QProgressDialog>

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

    // QSL services
    m_lotwService    = new LoTwService(this);
    m_eqslService    = new EqslService(this);
    m_qrzService     = new QrzService(this);
    m_clublogService = new ClubLogService(this);

    auto wireService = [this](QslService *svc) {
        connect(svc, &QslService::uploadFinished, this,
                [this, svc](const QList<Qso> &updated, const QStringList &errors) {
            qslUploadFinished(svc->displayName(), updated, errors);
        });
        connect(svc, &QslService::downloadFinished, this,
                [this, svc](const QList<Qso> &confirmed, const QStringList &errors) {
            qslDownloadFinished(svc->displayName(), confirmed, errors);
        });
        connect(svc, &QslService::uploadProgress, this, [this](int done, int total) {
            if (total > 0)
                statusBar()->showMessage(tr("Uploading… %1/%2").arg(done).arg(total));
        });
    };

    wireService(m_lotwService);
    wireService(m_eqslService);
    wireService(m_qrzService);
    wireService(m_clublogService);
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event)
{
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

    auto *lotwUpload   = new QAction(tr("Upload to &LoTW..."),        this);
    auto *lotwDownload = new QAction(tr("Download from &LoTW..."),     this);
    auto *eqslUpload   = new QAction(tr("Upload to &eQSL..."),         this);
    auto *eqslDownload = new QAction(tr("Download from e&QSL..."),     this);
    auto *qrzUpload    = new QAction(tr("Upload to &QRZ..."),          this);
    auto *qrzDownload  = new QAction(tr("Download from Q&RZ..."),      this);
    auto *clubUpload   = new QAction(tr("Upload to &ClubLog..."),      this);

    connect(lotwUpload,   &QAction::triggered, this, &MainWindow::onLotwUpload);
    connect(lotwDownload, &QAction::triggered, this, &MainWindow::onLotwDownload);
    connect(eqslUpload,   &QAction::triggered, this, &MainWindow::onEqslUpload);
    connect(eqslDownload, &QAction::triggered, this, &MainWindow::onEqslDownload);
    connect(qrzUpload,    &QAction::triggered, this, &MainWindow::onQrzUpload);
    connect(qrzDownload,  &QAction::triggered, this, &MainWindow::onQrzDownload);
    connect(clubUpload,   &QAction::triggered, this, &MainWindow::onClubLogUpload);

    qslMenu->addAction(lotwUpload);
    qslMenu->addAction(lotwDownload);
    qslMenu->addSeparator();
    qslMenu->addAction(eqslUpload);
    qslMenu->addAction(eqslDownload);
    qslMenu->addSeparator();
    qslMenu->addAction(qrzUpload);
    qslMenu->addAction(qrzDownload);
    qslMenu->addSeparator();
    qslMenu->addAction(clubUpload);

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
    m_logView->setModel(m_logModel);
    m_logView->setAlternatingRowColors(true);
    m_logView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_logView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_logView->setSortingEnabled(false);  // will enable once sort proxy is wired up
    m_logView->horizontalHeader()->setStretchLastSection(true);
    m_logView->horizontalHeader()->setHighlightSections(false);
    m_logView->verticalHeader()->hide();
    m_logView->verticalHeader()->setDefaultSectionSize(22);

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

    if (!backend->open(config)) {
        QMessageBox::critical(this, tr("Database Error"),
            tr("Could not open database:\n%1").arg(backend->lastError()));
        return;
    }

    if (!backend->initSchema()) {
        QMessageBox::critical(this, tr("Database Error"),
            tr("Schema initialisation failed:\n%1").arg(backend->lastError()));
        return;
    }

    m_db = std::move(backend);
    statusBar()->showMessage(tr("Database opened: %1").arg(dbPath), 4000);
    reloadLog();
}

void MainWindow::reloadLog()
{
    if (!m_db) return;
    m_logModel->setQsos(m_db->fetchQsos());
    updateQsoCount();
}

void MainWindow::updateQsoCount()
{
    if (!m_db) return;
    const int n = m_db->qsoCount();
    m_qsoCountLabel->setText(tr("QSOs: %1").arg(n < 0 ? 0 : n));
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
        if (!m_db->insertQso(qso)) {
            const QString err = m_db->lastError();
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

    const QList<Qso> qsos = m_db->fetchQsos();
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
    // Refresh status bar callsign if station settings changed
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

static QList<Qso> fetchAll(DatabaseInterface *db) { return db->fetchQsos(); }

void MainWindow::onLotwUpload()
{
    if (!m_db) return;
    if (!m_lotwService->isEnabled()) {
        QMessageBox::information(this, tr("LoTW"),
            tr("LoTW is not enabled. Enable it in Settings → QSL Services."));
        return;
    }
    m_lotwService->startUpload(fetchAll(m_db.get()));
}

void MainWindow::onLotwDownload()
{
    if (!m_lotwService->isEnabled()) {
        QMessageBox::information(this, tr("LoTW"),
            tr("LoTW is not enabled. Enable it in Settings → QSL Services."));
        return;
    }
    statusBar()->showMessage(tr("Downloading LoTW confirmations…"));
    m_lotwService->startDownload();
}

void MainWindow::onEqslUpload()
{
    if (!m_db) return;
    if (!m_eqslService->isEnabled()) {
        QMessageBox::information(this, tr("eQSL"),
            tr("eQSL is not enabled. Enable it in Settings → QSL Services."));
        return;
    }
    m_eqslService->startUpload(fetchAll(m_db.get()));
}

void MainWindow::onEqslDownload()
{
    if (!m_eqslService->isEnabled()) {
        QMessageBox::information(this, tr("eQSL"),
            tr("eQSL is not enabled. Enable it in Settings → QSL Services."));
        return;
    }
    statusBar()->showMessage(tr("Downloading eQSL confirmations…"));
    m_eqslService->startDownload();
}

void MainWindow::onQrzUpload()
{
    if (!m_db) return;
    if (!m_qrzService->isEnabled()) {
        QMessageBox::information(this, tr("QRZ"),
            tr("QRZ is not enabled. Enable it in Settings → QSL Services."));
        return;
    }
    m_qrzService->startUpload(fetchAll(m_db.get()));
}

void MainWindow::onQrzDownload()
{
    if (!m_qrzService->isEnabled()) {
        QMessageBox::information(this, tr("QRZ"),
            tr("QRZ is not enabled. Enable it in Settings → QSL Services."));
        return;
    }
    statusBar()->showMessage(tr("Downloading QRZ confirmations…"));
    m_qrzService->startDownload();
}

void MainWindow::onClubLogUpload()
{
    if (!m_db) return;
    if (!m_clublogService->isEnabled()) {
        QMessageBox::information(this, tr("ClubLog"),
            tr("ClubLog is not enabled. Enable it in Settings → QSL Services."));
        return;
    }
    m_clublogService->startUpload(fetchAll(m_db.get()));
}

void MainWindow::qslUploadFinished(const QString &serviceName,
                                     const QList<Qso> &updatedQsos,
                                     const QStringList &errors)
{
    if (!updatedQsos.isEmpty()) {
        for (const Qso &q : updatedQsos)
            m_db->updateQso(q);
        reloadLog();
    }

    QString msg = tr("%1 upload: %2 contact(s) sent.")
                      .arg(serviceName).arg(updatedQsos.size());
    if (!errors.isEmpty())
        msg += QLatin1Char('\n') + errors.join(QLatin1Char('\n'));

    if (errors.isEmpty() || !updatedQsos.isEmpty()) {
        QMessageBox::information(this, tr("%1 Upload").arg(serviceName), msg);
    } else {
        QMessageBox::warning(this, tr("%1 Upload").arg(serviceName), msg);
    }
}

void MainWindow::qslDownloadFinished(const QString &serviceName,
                                       const QList<Qso> &confirmed,
                                       const QStringList &errors)
{
    int updated = 0;

    if (!confirmed.isEmpty() && m_db) {
        const QList<Qso> local = fetchAll(m_db.get());

        for (const Qso &conf : confirmed) {
            // Match on callsign + date + band + mode
            const QDate confDate = conf.datetimeOn.toUTC().date();
            for (Qso match : local) {
                if (match.callsign.compare(conf.callsign, Qt::CaseInsensitive) != 0) continue;
                if (match.band.compare(conf.band, Qt::CaseInsensitive) != 0) continue;
                if (match.mode.compare(conf.mode, Qt::CaseInsensitive) != 0) continue;
                if (match.datetimeOn.toUTC().date() != confDate) continue;

                // Copy confirmed rcvd fields — service sets whatever is relevant
                if (conf.lotwQslRcvd == 'Y') { match.lotwQslRcvd = 'Y'; match.lotwRcvdDate = conf.lotwRcvdDate; }
                if (conf.eqslQslRcvd == 'Y') { match.eqslQslRcvd = 'Y'; match.eqslRcvdDate = conf.eqslRcvdDate; }
                if (conf.qrzQslRcvd  == 'Y') { match.qrzQslRcvd  = 'Y'; match.qrzRcvdDate  = conf.qrzRcvdDate; }

                m_db->updateQso(match);
                ++updated;
                break;
            }
        }

        if (updated > 0)
            reloadLog();
    }

    QString msg = tr("%1 download: %2 confirmation(s) matched.")
                      .arg(serviceName).arg(updated);
    if (!errors.isEmpty())
        msg += QLatin1Char('\n') + errors.join(QLatin1Char('\n'));

    if (errors.isEmpty() || updated > 0) {
        QMessageBox::information(this, tr("%1 Download").arg(serviceName), msg);
    } else {
        QMessageBox::warning(this, tr("%1 Download").arg(serviceName), msg);
    }

    statusBar()->clearMessage();
}

void MainWindow::onQsoReady(const Qso &qso)
{
    if (!m_db) return;

    Qso inserted = qso;
    if (!m_db->insertQso(inserted)) {
        statusBar()->showMessage(
            tr("Failed to log contact: %1").arg(m_db->lastError()), 5000);
        return;
    }

    m_logModel->prependQso(inserted);
    updateQsoCount();
    statusBar()->showMessage(
        tr("Logged: %1").arg(inserted.callsign), 3000);
}
