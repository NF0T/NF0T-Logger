#include "QslPage.h"
#include "app/settings/Settings.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

static QLineEdit *makePasswordField(QWidget *parent)
{
    auto *le = new QLineEdit(parent);
    le->setEchoMode(QLineEdit::Password);
    return le;
}

static QLabel *keychainNote(QWidget *parent)
{
    return new QLabel(QStringLiteral("<i>Stored securely in the system keychain.</i>"), parent);
}

QslPage::QslPage(QWidget *parent)
    : SettingsPage(parent)
{
    auto *tabs = new QTabWidget(this);

    // -----------------------------------------------------------------------
    // LoTW tab
    // -----------------------------------------------------------------------
    auto *lotwWidget = new QWidget;
    m_lotwEnabled         = new QCheckBox(tr("Enable LoTW integration"), lotwWidget);
    m_tqslPath            = new QLineEdit(lotwWidget);
    m_tqslPath->setPlaceholderText("/usr/bin/tqsl");
    m_lotwCallsign        = new QLineEdit(lotwWidget);
    m_lotwCallsign->setPlaceholderText(tr("Certificate callsign (for download login)"));
    m_lotwStationLocation = new QLineEdit(lotwWidget);
    m_lotwStationLocation->setPlaceholderText(tr("Exact name from TQSL Station Locations"));
    m_lotwPassword        = makePasswordField(lotwWidget);

    auto *tqslRow = new QHBoxLayout;
    tqslRow->addWidget(m_tqslPath, 1);
    auto *tqslBrowse = new QPushButton(tr("Browse…"), lotwWidget);
    connect(tqslBrowse, &QPushButton::clicked, this, [this]() {
        const QString p = QFileDialog::getOpenFileName(this, tr("TQSL Executable"));
        if (!p.isEmpty()) m_tqslPath->setText(p);
    });
    tqslRow->addWidget(tqslBrowse);

    auto *lotwForm = new QFormLayout(lotwWidget);
    lotwForm->addRow(m_lotwEnabled);
    lotwForm->addRow(tr("TQSL path:"),        tqslRow);
    lotwForm->addRow(tr("Callsign:"),         m_lotwCallsign);
    lotwForm->addRow(tr("Station location:"), m_lotwStationLocation);
    lotwForm->addRow(tr("Password:"),         m_lotwPassword);
    lotwForm->addRow(keychainNote(lotwWidget));

    // -----------------------------------------------------------------------
    // eQSL tab
    // -----------------------------------------------------------------------
    auto *eqslWidget = new QWidget;
    m_eqslEnabled  = new QCheckBox(tr("Enable eQSL integration"), eqslWidget);
    m_eqslUsername = new QLineEdit(eqslWidget);
    m_eqslNickname = new QLineEdit(eqslWidget);
    m_eqslNickname->setPlaceholderText(tr("eQSL nickname (optional)"));
    m_eqslPassword = makePasswordField(eqslWidget);

    auto *eqslForm = new QFormLayout(eqslWidget);
    eqslForm->addRow(m_eqslEnabled);
    eqslForm->addRow(tr("Username:"),  m_eqslUsername);
    eqslForm->addRow(tr("Nickname:"),  m_eqslNickname);
    eqslForm->addRow(tr("Password:"),  m_eqslPassword);
    eqslForm->addRow(keychainNote(eqslWidget));

    // -----------------------------------------------------------------------
    // QRZ tab
    // -----------------------------------------------------------------------
    auto *qrzWidget = new QWidget;
    m_qrzEnabled   = new QCheckBox(tr("Enable QRZ.com integration"), qrzWidget);
    m_qrzUsername  = new QLineEdit(qrzWidget);
    m_qrzApiKey    = makePasswordField(qrzWidget);

    auto *qrzForm = new QFormLayout(qrzWidget);
    qrzForm->addRow(m_qrzEnabled);
    qrzForm->addRow(tr("Username:"), m_qrzUsername);
    qrzForm->addRow(tr("API key:"),  m_qrzApiKey);
    qrzForm->addRow(keychainNote(qrzWidget));

    // -----------------------------------------------------------------------
    // ClubLog tab
    // -----------------------------------------------------------------------
    auto *clublogWidget  = new QWidget;
    m_clublogEnabled     = new QCheckBox(tr("Enable ClubLog integration"), clublogWidget);
    m_clublogEmail       = new QLineEdit(clublogWidget);
    m_clublogCallsign    = new QLineEdit(clublogWidget);
    m_clublogPassword    = makePasswordField(clublogWidget);
    m_clublogAppKey      = makePasswordField(clublogWidget);

    auto *clublogForm = new QFormLayout(clublogWidget);
    clublogForm->addRow(m_clublogEnabled);
    clublogForm->addRow(tr("Email:"),     m_clublogEmail);
    clublogForm->addRow(tr("Callsign:"),  m_clublogCallsign);
    clublogForm->addRow(tr("Password:"),  m_clublogPassword);
    clublogForm->addRow(tr("App key:"),   m_clublogAppKey);
    clublogForm->addRow(keychainNote(clublogWidget));

    tabs->addTab(lotwWidget,    tr("LoTW"));
    tabs->addTab(eqslWidget,    tr("eQSL"));
    tabs->addTab(qrzWidget,     tr("QRZ.com"));
    tabs->addTab(clublogWidget, tr("ClubLog"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(tabs);
}

void QslPage::load()
{
    const Settings &s = Settings::instance();

    m_lotwEnabled->setChecked(s.lotwEnabled());
    m_tqslPath->setText(s.lotwTqslPath());
    m_lotwCallsign->setText(s.lotwCallsign());
    m_lotwStationLocation->setText(s.lotwStationLocation());
    m_lotwPassword->setText(s.lotwPassword());

    m_eqslEnabled->setChecked(s.eqslEnabled());
    m_eqslUsername->setText(s.eqslUsername());
    m_eqslNickname->setText(s.eqslNickname());
    m_eqslPassword->setText(s.eqslPassword());

    m_qrzEnabled->setChecked(s.qrzEnabled());
    m_qrzUsername->setText(s.qrzUsername());
    m_qrzApiKey->setText(s.qrzApiKey());

    m_clublogEnabled->setChecked(s.clublogEnabled());
    m_clublogEmail->setText(s.clublogEmail());
    m_clublogCallsign->setText(s.clublogCallsign());
    m_clublogPassword->setText(s.clublogPassword());
    m_clublogAppKey->setText(s.clublogAppKey());
}

void QslPage::apply()
{
    Settings &s = Settings::instance();

    s.setLotwEnabled(m_lotwEnabled->isChecked());
    s.setLotwTqslPath(m_tqslPath->text().trimmed());
    s.setLotwCallsign(m_lotwCallsign->text().toUpper().trimmed());
    s.setLotwStationLocation(m_lotwStationLocation->text().trimmed());
    s.setLotwPassword(m_lotwPassword->text());

    s.setEqslEnabled(m_eqslEnabled->isChecked());
    s.setEqslUsername(m_eqslUsername->text().trimmed());
    s.setEqslNickname(m_eqslNickname->text().trimmed());
    s.setEqslPassword(m_eqslPassword->text());

    s.setQrzEnabled(m_qrzEnabled->isChecked());
    s.setQrzUsername(m_qrzUsername->text().trimmed());
    s.setQrzApiKey(m_qrzApiKey->text().trimmed());

    s.setClublogEnabled(m_clublogEnabled->isChecked());
    s.setClublogEmail(m_clublogEmail->text().trimmed());
    s.setClublogCallsign(m_clublogCallsign->text().toUpper().trimmed());
    s.setClublogPassword(m_clublogPassword->text());
    s.setClublogAppKey(m_clublogAppKey->text().trimmed());
}
