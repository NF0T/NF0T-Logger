// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <QDialog>
#include <QVariantMap>

class DatabaseInterface;
class QCheckBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QStackedWidget;

/// Guided dialog for migrating all QSOs from the active backend to a new backend.
/// Presents three sequential pages: target configuration, dry-run results,
/// and migration progress/result. The active backend is always the source;
/// the user configures a clean (empty) target.
class MigrateDatabaseDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MigrateDatabaseDialog(DatabaseInterface *source, QWidget *parent = nullptr);

    /// True if migration succeeded and the user checked "Switch active database".
    bool        shouldSwitchBackend() const;
    QString     targetBackendKey()    const { return m_targetKey; }
    QVariantMap targetConfig()        const { return m_targetConfig; }

private slots:
    void onBackendToggled();
    void onDryRun();
    void onMigrate();
    void onBack();

private:
    enum Page { Configure = 0, DryRun = 1, Result = 2 };
    void goTo(Page p);

    QVariantMap buildTargetConfig() const;

    // Page 0 — configure target
    QWidget      *m_configurePage    = nullptr;
    QLabel       *m_sourceInfoLabel  = nullptr;
    QRadioButton *m_sqliteRadio      = nullptr;
    QRadioButton *m_mariadbRadio     = nullptr;
    QGroupBox    *m_sqliteGroup      = nullptr;
    QLineEdit    *m_sqlitePath       = nullptr;
    QGroupBox    *m_mariadbGroup     = nullptr;
    QLineEdit    *m_dbHost           = nullptr;
    QSpinBox     *m_dbPort           = nullptr;
    QLineEdit    *m_dbDatabase       = nullptr;
    QLineEdit    *m_dbUsername       = nullptr;
    QLineEdit    *m_dbPassword       = nullptr;
    QLabel       *m_configErrorLabel = nullptr;
    QPushButton  *m_dryRunBtn        = nullptr;

    // Page 1 — dry-run results
    QWidget     *m_dryRunPage        = nullptr;
    QLabel      *m_dryRunSummary     = nullptr;
    QPushButton *m_backBtn           = nullptr;
    QPushButton *m_migrateBtn        = nullptr;

    // Page 2 — progress / result
    QWidget      *m_resultPage       = nullptr;
    QProgressBar *m_progressBar      = nullptr;
    QLabel       *m_progressLabel    = nullptr;
    QLabel       *m_resultLabel      = nullptr;
    QCheckBox    *m_switchCheckBox   = nullptr;
    QPushButton  *m_cancelBtn        = nullptr;
    QPushButton  *m_finishBtn        = nullptr;

    QStackedWidget *m_stack = nullptr;

    // State
    DatabaseInterface *m_source      = nullptr;
    QString            m_targetKey;
    QVariantMap        m_targetConfig;
    int                m_dryRunTransferred = 0;
    int                m_dryRunDuplicates  = 0;
    bool               m_migrationDone     = false;
    bool               m_cancelled         = false;
};
