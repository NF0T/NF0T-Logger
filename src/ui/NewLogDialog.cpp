// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "NewLogDialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

NewLogDialog::NewLogDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New Log"));
    setMinimumWidth(480);

    m_archiveRadio = new QRadioButton(tr("Archive and start fresh"), this);
    m_discardRadio = new QRadioButton(tr("Start fresh (discard current log)"), this);
    m_archiveRadio->setChecked(true);

    auto *modeGroup = new QGroupBox(tr("What do you want to do with the current log?"), this);
    auto *modeLayout = new QVBoxLayout(modeGroup);
    modeLayout->addWidget(m_archiveRadio);
    modeLayout->addWidget(m_discardRadio);

    m_pathLabel = new QLabel(tr("Export to:"), this);
    m_pathEdit  = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(tr("Choose a file…"));
    m_browseBtn = new QPushButton(tr("Browse…"), this);

    auto *pathLayout = new QGridLayout;
    pathLayout->addWidget(m_pathLabel,  0, 0);
    pathLayout->addWidget(m_pathEdit,   0, 1);
    pathLayout->addWidget(m_browseBtn,  0, 2);
    pathLayout->setColumnStretch(1, 1);

    m_buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto *root = new QVBoxLayout(this);
    root->addWidget(modeGroup);
    root->addLayout(pathLayout);
    root->addWidget(m_buttons);

    connect(m_archiveRadio, &QRadioButton::toggled, this, &NewLogDialog::onModeToggled);
    connect(m_browseBtn,    &QPushButton::clicked,  this, &NewLogDialog::onBrowse);
    connect(m_pathEdit,     &QLineEdit::textChanged, this, &NewLogDialog::onPathChanged);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    onModeToggled();
}

NewLogDialog::Mode NewLogDialog::mode() const
{
    return m_archiveRadio->isChecked() ? Mode::Archive : Mode::Discard;
}

QString NewLogDialog::exportPath() const
{
    return m_pathEdit->text().trimmed();
}

void NewLogDialog::onModeToggled()
{
    const bool archive = m_archiveRadio->isChecked();
    m_pathLabel->setEnabled(archive);
    m_pathEdit->setEnabled(archive);
    m_browseBtn->setEnabled(archive);
    updateOkButton();
}

void NewLogDialog::onBrowse()
{
    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Export ADIF Archive"),
        {},
        tr("ADIF Files (*.adi *.adif);;All Files (*)"));
    if (!path.isEmpty())
        m_pathEdit->setText(path);
}

void NewLogDialog::onPathChanged()
{
    updateOkButton();
}

void NewLogDialog::updateOkButton()
{
    const bool ok = m_discardRadio->isChecked()
                    || !m_pathEdit->text().trimmed().isEmpty();
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(ok);
}
