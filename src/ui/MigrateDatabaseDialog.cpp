// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "MigrateDatabaseDialog.h"

#include "app/settings/Settings.h"
#include "database/DatabaseInterface.h"
#include "database/MariaDbBackend.h"
#include "database/SqliteBackend.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

MigrateDatabaseDialog::MigrateDatabaseDialog(DatabaseInterface *source, QWidget *parent)
    : QDialog(parent)
    , m_source(source)
{
    setWindowTitle(tr("Migrate Database"));
    setMinimumWidth(500);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    const Settings &s   = Settings::instance();
    const QString srcKey = s.dbBackend();
    const bool srcIsSqlite = (srcKey == QLatin1String("sqlite"));

    // Source info
    QString srcDesc;
    if (srcIsSqlite)
        srcDesc = tr("SQLite: %1").arg(s.dbSqlitePath());
    else
        srcDesc = tr("MariaDB: %1/%2").arg(s.dbMariadbHost(), s.dbMariadbDatabase());
    const int srcCount = m_source->qsoCount().value_or(-1);
    if (srcCount >= 0)
        srcDesc += tr(" — %1 QSOs").arg(srcCount);

    // -----------------------------------------------------------------------
    // Page 0 — Configure Target
    // -----------------------------------------------------------------------
    m_configurePage = new QWidget;

    m_sourceInfoLabel = new QLabel(srcDesc, m_configurePage);
    m_sourceInfoLabel->setWordWrap(true);
    auto *srcGroup = new QGroupBox(tr("Source (read-only)"), m_configurePage);
    auto *srcLayout = new QVBoxLayout(srcGroup);
    srcLayout->addWidget(m_sourceInfoLabel);

    // Backend selector — disable the source backend option
    m_sqliteRadio  = new QRadioButton(tr("SQLite"),  m_configurePage);
    m_mariadbRadio = new QRadioButton(tr("MariaDB"), m_configurePage);
    m_sqliteRadio ->setEnabled(!srcIsSqlite);
    m_mariadbRadio->setEnabled(srcIsSqlite);
    if (srcIsSqlite)
        m_mariadbRadio->setChecked(true);
    else
        m_sqliteRadio->setChecked(true);

    auto *backendRow = new QHBoxLayout;
    backendRow->addWidget(m_sqliteRadio);
    backendRow->addWidget(m_mariadbRadio);
    backendRow->addStretch();

    // SQLite target group
    m_sqliteGroup = new QGroupBox(tr("SQLite"), m_configurePage);
    m_sqlitePath  = new QLineEdit(m_sqliteGroup);
    auto *browseBtn = new QPushButton(tr("Browse…"), m_sqliteGroup);
    browseBtn->setFixedWidth(80);
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Select target SQLite database"), m_sqlitePath->text(),
            tr("SQLite Databases (*.db);;All Files (*)"));
        if (!path.isEmpty())
            m_sqlitePath->setText(path);
    });
    auto *sqliteRow = new QHBoxLayout;
    sqliteRow->addWidget(m_sqlitePath);
    sqliteRow->addWidget(browseBtn);
    auto *sqliteForm = new QVBoxLayout(m_sqliteGroup);
    sqliteForm->addLayout(sqliteRow);
    sqliteForm->addWidget(new QLabel(
        tr("The file will be created if it does not exist."), m_sqliteGroup));

    // MariaDB target group
    m_mariadbGroup = new QGroupBox(tr("MariaDB"), m_configurePage);
    m_dbHost       = new QLineEdit(m_mariadbGroup);
    m_dbPort       = new QSpinBox(m_mariadbGroup);
    m_dbPort->setRange(1, 65535);
    m_dbPort->setValue(3306);
    m_dbDatabase   = new QLineEdit(m_mariadbGroup);
    m_dbUsername   = new QLineEdit(m_mariadbGroup);
    m_dbPassword   = new QLineEdit(m_mariadbGroup);
    m_dbPassword->setEchoMode(QLineEdit::Password);

    auto *mariaForm = new QFormLayout(m_mariadbGroup);
    mariaForm->addRow(tr("Host:"),     m_dbHost);
    mariaForm->addRow(tr("Port:"),     m_dbPort);
    mariaForm->addRow(tr("Database:"), m_dbDatabase);
    mariaForm->addRow(tr("Username:"), m_dbUsername);
    mariaForm->addRow(tr("Password:"), m_dbPassword);
    mariaForm->addRow(new QLabel(
        tr("The database must already exist on the server."), m_mariadbGroup));

    // Pre-fill MariaDB fields from stored settings
    m_dbHost->setText(s.dbMariadbHost());
    m_dbPort->setValue(s.dbMariadbPort());
    m_dbDatabase->setText(s.dbMariadbDatabase());
    m_dbUsername->setText(s.dbMariadbUsername());
    m_dbPassword->setText(s.dbMariadbPassword());

    // Pre-fill SQLite suggested path
    const QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    m_sqlitePath->setText(dataDir + QStringLiteral("/log_migrated.db"));

    m_configErrorLabel = new QLabel(m_configurePage);
    m_configErrorLabel->setStyleSheet(QStringLiteral("color: red;"));
    m_configErrorLabel->setWordWrap(true);
    m_configErrorLabel->hide();

    m_dryRunBtn = new QPushButton(tr("Dry Run →"), m_configurePage);
    connect(m_dryRunBtn, &QPushButton::clicked, this, &MigrateDatabaseDialog::onDryRun);

    auto *configBtnRow = new QHBoxLayout;
    configBtnRow->addStretch();
    configBtnRow->addWidget(m_dryRunBtn);

    auto *configLayout = new QVBoxLayout(m_configurePage);
    configLayout->addWidget(srcGroup);
    configLayout->addSpacing(8);
    configLayout->addWidget(new QLabel(tr("Target Backend:"), m_configurePage));
    configLayout->addLayout(backendRow);
    configLayout->addWidget(m_sqliteGroup);
    configLayout->addWidget(m_mariadbGroup);
    configLayout->addWidget(m_configErrorLabel);
    configLayout->addStretch();
    configLayout->addLayout(configBtnRow);

    connect(m_sqliteRadio,  &QRadioButton::toggled, this, &MigrateDatabaseDialog::onBackendToggled);
    connect(m_mariadbRadio, &QRadioButton::toggled, this, &MigrateDatabaseDialog::onBackendToggled);
    onBackendToggled();

    // -----------------------------------------------------------------------
    // Page 1 — Dry-Run Results
    // -----------------------------------------------------------------------
    m_dryRunPage    = new QWidget;
    m_dryRunSummary = new QLabel(m_dryRunPage);
    m_dryRunSummary->setWordWrap(true);

    m_backBtn    = new QPushButton(tr("← Back"), m_dryRunPage);
    m_migrateBtn = new QPushButton(tr("Migrate →"), m_dryRunPage);
    m_migrateBtn->setEnabled(false);
    connect(m_backBtn,    &QPushButton::clicked, this, &MigrateDatabaseDialog::onBack);
    connect(m_migrateBtn, &QPushButton::clicked, this, &MigrateDatabaseDialog::onMigrate);

    auto *dryBtnRow = new QHBoxLayout;
    dryBtnRow->addWidget(m_backBtn);
    dryBtnRow->addStretch();
    dryBtnRow->addWidget(m_migrateBtn);

    auto *dryLayout = new QVBoxLayout(m_dryRunPage);
    dryLayout->addWidget(new QLabel(
        QStringLiteral("<b>") + tr("Dry-Run Results") + QStringLiteral("</b>"),
        m_dryRunPage));
    dryLayout->addSpacing(8);
    dryLayout->addWidget(m_dryRunSummary);
    dryLayout->addStretch();
    dryLayout->addLayout(dryBtnRow);

    // -----------------------------------------------------------------------
    // Page 2 — Progress / Result
    // -----------------------------------------------------------------------
    m_resultPage   = new QWidget;
    m_progressBar  = new QProgressBar(m_resultPage);
    m_progressBar->setTextVisible(true);
    m_progressLabel = new QLabel(m_resultPage);
    m_resultLabel   = new QLabel(m_resultPage);
    m_resultLabel->setWordWrap(true);
    m_resultLabel->hide();

    m_switchCheckBox = new QCheckBox(tr("Switch active database to this backend"), m_resultPage);
    m_switchCheckBox->hide();

    m_cancelBtn = new QPushButton(tr("Cancel"), m_resultPage);
    m_finishBtn = new QPushButton(tr("Finish"), m_resultPage);
    m_finishBtn->hide();
    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        m_cancelled = true;
        m_cancelBtn->setEnabled(false);
    });
    connect(m_finishBtn, &QPushButton::clicked, this, &QDialog::accept);

    auto *resultBtnRow = new QHBoxLayout;
    resultBtnRow->addStretch();
    resultBtnRow->addWidget(m_cancelBtn);
    resultBtnRow->addWidget(m_finishBtn);

    auto *resultLayout = new QVBoxLayout(m_resultPage);
    resultLayout->addWidget(m_progressBar);
    resultLayout->addWidget(m_progressLabel);
    resultLayout->addWidget(m_resultLabel);
    resultLayout->addWidget(m_switchCheckBox);
    resultLayout->addStretch();
    resultLayout->addLayout(resultBtnRow);

    // -----------------------------------------------------------------------
    // Stack + outer layout
    // -----------------------------------------------------------------------
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_configurePage);  // 0
    m_stack->addWidget(m_dryRunPage);     // 1
    m_stack->addWidget(m_resultPage);     // 2

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->addWidget(m_stack);

    // Close button in result page handled by Finish; reject() closes dialog on X
    // No default QDialogButtonBox — we manage navigation manually.
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void MigrateDatabaseDialog::goTo(Page p)
{
    m_stack->setCurrentIndex(static_cast<int>(p));
}

void MigrateDatabaseDialog::onBackendToggled()
{
    m_sqliteGroup ->setVisible(m_sqliteRadio->isChecked());
    m_mariadbGroup->setVisible(m_mariadbRadio->isChecked());
    m_configErrorLabel->hide();
}

QVariantMap MigrateDatabaseDialog::buildTargetConfig() const
{
    if (m_sqliteRadio->isChecked()) {
        return {{"path", m_sqlitePath->text().trimmed()}};
    }
    return {
        {"host",     m_dbHost->text().trimmed()},
        {"port",     m_dbPort->value()},
        {"database", m_dbDatabase->text().trimmed()},
        {"username", m_dbUsername->text().trimmed()},
        {"password", m_dbPassword->text()},
    };
}

// Opens a fresh backend instance using the current form values.
// Returns nullptr and sets errorOut on failure.
static std::unique_ptr<DatabaseInterface> openTarget(const QString &key,
                                                      const QVariantMap &config,
                                                      QString &errorOut)
{
    std::unique_ptr<DatabaseInterface> target;
    if (key == QLatin1String("sqlite"))
        target = std::make_unique<SqliteBackend>();
    else
        target = std::make_unique<MariaDbBackend>();

    if (auto r = target->open(config); !r) {
        errorOut = r.error();
        return nullptr;
    }
    if (auto r = target->initSchema(); !r) {
        errorOut = r.error();
        return nullptr;
    }
    return target;
}

// ---------------------------------------------------------------------------
// Dry run
// ---------------------------------------------------------------------------

void MigrateDatabaseDialog::onDryRun()
{
    m_configErrorLabel->hide();
    m_dryRunBtn->setEnabled(false);

    const QString key    = m_sqliteRadio->isChecked()
                           ? QStringLiteral("sqlite") : QStringLiteral("mariadb");
    const QVariantMap cfg = buildTargetConfig();

    // Validate path not empty for SQLite
    if (key == QLatin1String("sqlite") && cfg.value("path").toString().isEmpty()) {
        m_configErrorLabel->setText(tr("Please enter a target file path."));
        m_configErrorLabel->show();
        m_dryRunBtn->setEnabled(true);
        return;
    }

    QString openErr;
    auto target = openTarget(key, cfg, openErr);
    if (!target) {
        m_configErrorLabel->setText(tr("Could not connect to target: %1").arg(openErr));
        m_configErrorLabel->show();
        m_dryRunBtn->setEnabled(true);
        return;
    }

    // Refuse non-empty target
    const int targetCount = target->qsoCount().value_or(-1);
    if (targetCount > 0) {
        m_configErrorLabel->setText(
            tr("Target database already contains %1 QSO(s). "
               "Migration requires a clean target. "
               "To merge logbooks, use File → Export ADIF on the source "
               "and File → Import ADIF on the target.")
                .arg(targetCount));
        m_configErrorLabel->show();
        m_dryRunBtn->setEnabled(true);
        return;
    }

    // Transaction-based dry run: insert everything, then roll back
    if (auto r = target->beginTransaction(); !r) {
        m_configErrorLabel->setText(
            tr("Could not begin dry-run transaction: %1").arg(r.error()));
        m_configErrorLabel->show();
        m_dryRunBtn->setEnabled(true);
        return;
    }

    const auto fetchResult = m_source->fetchQsos();
    if (!fetchResult) {
        target->rollbackTransaction();
        m_configErrorLabel->setText(
            tr("Could not read source QSOs: %1").arg(fetchResult.error()));
        m_configErrorLabel->show();
        m_dryRunBtn->setEnabled(true);
        return;
    }

    const QList<Qso> &qsos = *fetchResult;
    int transferred = 0, duplicates = 0, errors = 0;

    for (Qso qso : qsos) {
        qso.id = 0;  // reset so target assigns a new id
        if (auto r = target->insertQso(qso); r) {
            ++transferred;
        } else {
            const QString &err = r.error();
            if (err.contains(QLatin1String("UNIQUE"), Qt::CaseInsensitive) ||
                err.contains(QLatin1String("Duplicate"), Qt::CaseInsensitive))
                ++duplicates;
            else
                ++errors;
        }
    }

    target->rollbackTransaction();
    // target goes out of scope here — its connection is released

    m_dryRunTransferred = transferred;
    m_dryRunDuplicates  = duplicates;

    // Store for use by onMigrate
    m_targetKey    = key;
    m_targetConfig = cfg;

    // Build summary text
    QString summary;
    if (errors == 0) {
        summary = tr("<b>%1</b> QSO(s) would be transferred.<br>")
                      .arg(transferred);
        if (duplicates > 0)
            summary += tr("<b>%1</b> duplicate(s) found and would be skipped.<br>")
                           .arg(duplicates);
        summary += tr("<br>No errors detected. Ready to migrate.");
    } else {
        summary = tr("<b>%1</b> QSO(s) would be transferred.<br>"
                     "<b>%2</b> duplicate(s) would be skipped.<br>"
                     "<font color='red'><b>%3</b> error(s) detected.</font><br><br>"
                     "Review the source database before proceeding.")
                      .arg(transferred).arg(duplicates).arg(errors);
    }

    m_dryRunSummary->setText(summary);
    m_migrateBtn->setEnabled(errors == 0 && transferred > 0);
    m_dryRunBtn->setEnabled(true);

    goTo(DryRun);
}

// ---------------------------------------------------------------------------
// Back (dry-run → configure)
// ---------------------------------------------------------------------------

void MigrateDatabaseDialog::onBack()
{
    goTo(Configure);
}

// ---------------------------------------------------------------------------
// Migration
// ---------------------------------------------------------------------------

void MigrateDatabaseDialog::onMigrate()
{
    goTo(Result);

    const int total = m_dryRunTransferred + m_dryRunDuplicates;
    m_progressBar->setRange(0, total > 0 ? total : 1);
    m_progressBar->setValue(0);
    m_progressLabel->setText(tr("Opening target database…"));

    QString openErr;
    auto target = openTarget(m_targetKey, m_targetConfig, openErr);
    if (!target) {
        m_progressLabel->setText(tr("Failed to open target: %1").arg(openErr));
        m_cancelBtn->hide();
        m_finishBtn->show();
        return;
    }

    const auto fetchResult = m_source->fetchQsos();
    if (!fetchResult) {
        m_progressLabel->setText(
            tr("Could not read source QSOs: %1").arg(fetchResult.error()));
        m_cancelBtn->hide();
        m_finishBtn->show();
        return;
    }

    const QList<Qso> &qsos = *fetchResult;
    int transferred = 0, duplicates = 0, errors = 0;
    int processed   = 0;

    m_progressLabel->setText(tr("Migrating…"));

    for (Qso qso : qsos) {
        if (m_cancelled)
            break;

        qso.id = 0;
        if (auto r = target->insertQso(qso); r) {
            ++transferred;
        } else {
            const QString &err = r.error();
            if (err.contains(QLatin1String("UNIQUE"), Qt::CaseInsensitive) ||
                err.contains(QLatin1String("Duplicate"), Qt::CaseInsensitive))
                ++duplicates;
            else
                ++errors;
        }

        ++processed;
        m_progressBar->setValue(processed);

        if (processed % 100 == 0)
            qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    // target closes and releases its connection when it goes out of scope here
    m_migrationDone = !m_cancelled;

    m_progressBar->setValue(m_progressBar->maximum());

    QString result;
    if (m_cancelled) {
        result = tr("<b>Migration cancelled.</b><br>"
                    "Transferred so far: %1 &nbsp; Duplicates skipped: %2 &nbsp; Errors: %3<br><br>"
                    "The target database may be partially populated.")
                     .arg(transferred).arg(duplicates).arg(errors);
        m_switchCheckBox->hide();
    } else {
        result = tr("<b>Migration complete.</b><br><br>"
                    "Transferred: &nbsp; <b>%1</b><br>"
                    "Duplicates skipped: <b>%2</b><br>"
                    "Errors: &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; <b>%3</b>")
                     .arg(transferred).arg(duplicates).arg(errors);
        m_switchCheckBox->setChecked(errors == 0);
        m_switchCheckBox->show();
    }

    m_resultLabel->setText(result);
    m_resultLabel->show();
    m_progressLabel->hide();
    m_cancelBtn->hide();
    m_finishBtn->show();
}

// ---------------------------------------------------------------------------
// Result accessors
// ---------------------------------------------------------------------------

bool MigrateDatabaseDialog::shouldSwitchBackend() const
{
    return m_migrationDone && m_switchCheckBox->isChecked();
}
