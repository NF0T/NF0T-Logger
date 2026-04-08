#include "DatabasePage.h"
#include "app/settings/Settings.h"

#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

DatabasePage::DatabasePage(QWidget *parent)
    : SettingsPage(parent)
{
    // Backend selector
    m_sqliteRadio  = new QRadioButton(tr("SQLite (recommended for single-user)"), this);
    m_mariadbRadio = new QRadioButton(tr("MariaDB / MySQL (multi-user / network)"), this);
    m_sqliteRadio->setChecked(true);

    connect(m_sqliteRadio,  &QRadioButton::toggled, this, &DatabasePage::onBackendToggled);

    // SQLite group
    m_sqliteGroup = new QGroupBox(tr("SQLite"), this);
    m_sqlitePath  = new QLineEdit(m_sqliteGroup);
    const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                                + "/log.db";
    m_sqlitePath->setPlaceholderText(defaultPath);

    auto *browseBtn = new QPushButton(tr("Browse…"), m_sqliteGroup);
    connect(browseBtn, &QPushButton::clicked, this, [this, defaultPath]() {
        const QString p = QFileDialog::getSaveFileName(
            this, tr("SQLite Database File"), m_sqlitePath->text().isEmpty() ? defaultPath : m_sqlitePath->text(),
            tr("SQLite Files (*.db *.sqlite);;All Files (*)"));
        if (!p.isEmpty()) m_sqlitePath->setText(p);
    });

    auto *sqliteRow = new QHBoxLayout;
    sqliteRow->addWidget(new QLabel(tr("File:"), m_sqliteGroup));
    sqliteRow->addWidget(m_sqlitePath, 1);
    sqliteRow->addWidget(browseBtn);

    auto *sqliteLayout = new QVBoxLayout(m_sqliteGroup);
    sqliteLayout->addLayout(sqliteRow);
    sqliteLayout->addWidget(new QLabel(tr("Leave blank to use the default location."), m_sqliteGroup));

    // MariaDB group
    m_mariadbGroup = new QGroupBox(tr("MariaDB / MySQL"), this);
    m_dbHost       = new QLineEdit(m_mariadbGroup);
    m_dbPort       = new QSpinBox(m_mariadbGroup);
    m_dbPort->setRange(1, 65535);
    m_dbPort->setValue(3306);
    m_dbDatabase   = new QLineEdit(m_mariadbGroup);
    m_dbUsername   = new QLineEdit(m_mariadbGroup);
    m_dbPassword   = new QLineEdit(m_mariadbGroup);
    m_dbPassword->setEchoMode(QLineEdit::Password);

    auto *dbForm = new QFormLayout(m_mariadbGroup);
    dbForm->addRow(tr("Host:"),     m_dbHost);
    dbForm->addRow(tr("Port:"),     m_dbPort);
    dbForm->addRow(tr("Database:"), m_dbDatabase);
    dbForm->addRow(tr("Username:"), m_dbUsername);
    dbForm->addRow(tr("Password:"), m_dbPassword);
    dbForm->addRow(new QLabel(tr("Password is stored in the system keychain."), m_mariadbGroup));

    // Root layout
    auto *root = new QVBoxLayout(this);
    root->addWidget(m_sqliteRadio);
    root->addWidget(m_mariadbRadio);
    root->addWidget(m_sqliteGroup);
    root->addWidget(m_mariadbGroup);
    root->addWidget(new QLabel(
        tr("<i>Database changes take effect after restarting the application.</i>"), this));
    root->addStretch();

    onBackendToggled();
}

void DatabasePage::onBackendToggled()
{
    m_sqliteGroup->setEnabled(m_sqliteRadio->isChecked());
    m_mariadbGroup->setEnabled(m_mariadbRadio->isChecked());
}

void DatabasePage::load()
{
    const Settings &s = Settings::instance();
    const bool isSqlite = (s.dbBackend() != "mariadb");
    m_sqliteRadio->setChecked(isSqlite);
    m_mariadbRadio->setChecked(!isSqlite);
    m_sqlitePath->setText(s.dbSqlitePath());
    m_dbHost->setText(s.dbMariadbHost());
    m_dbPort->setValue(s.dbMariadbPort());
    m_dbDatabase->setText(s.dbMariadbDatabase());
    m_dbUsername->setText(s.dbMariadbUsername());
    m_dbPassword->setText(s.dbMariadbPassword());
    onBackendToggled();
}

void DatabasePage::apply()
{
    Settings &s = Settings::instance();
    s.setDbBackend(m_sqliteRadio->isChecked() ? "sqlite" : "mariadb");
    s.setDbSqlitePath(m_sqlitePath->text().trimmed());
    s.setDbMariadbHost(m_dbHost->text().trimmed());
    s.setDbMariadbPort(m_dbPort->value());
    s.setDbMariadbDatabase(m_dbDatabase->text().trimmed());
    s.setDbMariadbUsername(m_dbUsername->text().trimmed());
    s.setDbMariadbPassword(m_dbPassword->text());
}
