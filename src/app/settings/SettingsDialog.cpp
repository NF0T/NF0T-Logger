#include "SettingsDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "pages/SettingsPage.h"
#include "pages/StationPage.h"
#include "pages/EquipmentPage.h"
#include "pages/DatabasePage.h"
#include "pages/RadioPage.h"
#include "pages/QslPage.h"

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setMinimumSize(780, 520);
    resize(860, 580);

    // --- Navigation list ---
    m_nav = new QListWidget(this);
    m_nav->setMaximumWidth(160);
    m_nav->setIconSize(QSize(24, 24));

    // --- Stacked pages ---
    m_stack = new QStackedWidget(this);

    auto addPage = [&](SettingsPage *page) {
        m_pages.append(page);
        m_stack->addWidget(page);
        auto *item = new QListWidgetItem(page->pageTitle(), m_nav);
        item->setSizeHint(QSize(0, 36));
    };

    addPage(new StationPage(this));
    addPage(new EquipmentPage(this));
    addPage(new DatabasePage(this));
    addPage(new RadioPage(this));
    addPage(new QslPage(this));

    // Load all pages from current settings
    for (SettingsPage *p : m_pages)
        p->load();

    connect(m_nav, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    m_nav->setCurrentRow(0);

    // --- Button box ---
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);

    connect(buttons, &QDialogButtonBox::accepted,  this, &SettingsDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected,  this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &SettingsDialog::onApply);

    // --- Layout ---
    auto *contentLayout = new QHBoxLayout;
    contentLayout->addWidget(m_nav);
    contentLayout->addWidget(m_stack, 1);

    auto *root = new QVBoxLayout(this);
    root->addLayout(contentLayout, 1);
    root->addWidget(buttons);
}

void SettingsDialog::showPage(Page page)
{
    m_nav->setCurrentRow(static_cast<int>(page));
}

void SettingsDialog::applyAll()
{
    for (SettingsPage *p : m_pages)
        p->apply();
}

void SettingsDialog::onApply()
{
    applyAll();
}

void SettingsDialog::onAccepted()
{
    applyAll();
    accept();
}
