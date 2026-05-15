// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <QDialog>
#include <QString>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;

/// Dialog presented by File → New Log.
///
/// The user chooses between two modes:
///   Archive  — export current QSOs to an ADIF file, then clear the log.
///   Discard  — clear the log immediately after confirmation.
///
/// Call exec(); if it returns QDialog::Accepted, read mode() and exportPath().
class NewLogDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode { Archive, Discard };

    explicit NewLogDialog(QWidget *parent = nullptr);

    Mode    mode()       const;
    QString exportPath() const;

private slots:
    void onModeToggled();
    void onBrowse();
    void onPathChanged();

private:
    void updateOkButton();

    QRadioButton    *m_archiveRadio = nullptr;
    QRadioButton    *m_discardRadio = nullptr;
    QLabel          *m_pathLabel    = nullptr;
    QLineEdit       *m_pathEdit     = nullptr;
    QPushButton     *m_browseBtn    = nullptr;
    QDialogButtonBox *m_buttons     = nullptr;
};
