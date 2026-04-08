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
#include <QVBoxLayout>
#include <QWidget>

#include <QFileDialog>
#include <QProgressDialog>

#include "core/adif/AdifParser.h"
#include "core/adif/AdifWriter.h"
#include "core/logbook/QsoTableModel.h"
#include "database/SqliteBackend.h"

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

    openDefaultDatabase();
}

MainWindow::~MainWindow() = default;

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
    radioMenu->addAction(tr("Connect Hamlib..."))->setEnabled(false);
    radioMenu->addAction(tr("Connect TCI..."))->setEnabled(false);
    radioMenu->addSeparator();
    radioMenu->addAction(tr("Disconnect"))->setEnabled(false);

    // --- QSL ---
    QMenu *qslMenu = menuBar()->addMenu(tr("&QSL"));
    qslMenu->addAction(tr("Upload to LoTW..."))->setEnabled(false);
    qslMenu->addAction(tr("Download from LoTW..."))->setEnabled(false);
    qslMenu->addSeparator();
    qslMenu->addAction(tr("Upload to eQSL..."))->setEnabled(false);
    qslMenu->addAction(tr("Download from eQSL..."))->setEnabled(false);
    qslMenu->addSeparator();
    qslMenu->addAction(tr("Upload to QRZ..."))->setEnabled(false);
    qslMenu->addAction(tr("Upload to ClubLog..."))->setEnabled(false);

    // --- Help ---
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    m_aboutAction = new QAction(tr("&About NF0T Logger"), this);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
    helpMenu->addAction(m_aboutAction);
}

void MainWindow::setupToolBar()
{
    QToolBar *toolBar = addToolBar(tr("Main Toolbar"));
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

    // Bottom pane — QSO entry widget (placeholder)
    QWidget *entryPlaceholder = new QWidget(m_splitter);
    entryPlaceholder->setMinimumHeight(120);
    QVBoxLayout *entryLayout = new QVBoxLayout(entryPlaceholder);
    entryLayout->addWidget(new QLabel(tr("QSO Entry — coming soon"), entryPlaceholder));

    m_splitter->addWidget(m_logView);
    m_splitter->addWidget(entryPlaceholder);
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
    QMessageBox::information(this, tr("Settings"), tr("Settings dialog coming soon."));
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
