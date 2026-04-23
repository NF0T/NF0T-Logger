// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <memory>
#include <QList>
#include <QMainWindow>
#include <QStringList>

#include "core/logbook/Qso.h"

class QAction;
class QLabel;
class QSplitter;
class QTableView;

class ClubLogService;
class DatabaseInterface;
class DigitalListenerService;
class EqslService;
class HamlibBackend;
class LoTwService;
class QrzService;
class QslService;
class LogFilterBar;
class QsoEntryPanel;
class RadioPanel;
class QsoTableModel;
class RadioBackend;
class TciBackend;
class WsjtxService;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onNewLog();
    void onImportAdif();
    void onExportAdif();
    void onSettingsDialog();
    void onAbout();
    void onQsoReady(const Qso &qso);
    void onEditQso(const QModelIndex &index);
    void onDeleteSelectedQso();
    void onExportSelectedQsos();
    void onWhatsNew();
    void onConnectHamlib();
    void onConnectTci();
    void onDisconnectRadio();
    // QSL
    void onQslDownload();
    void onQslUpload();

private:
    // QSL helpers — apply confirmed/updated QSOs to the database
    void applyDownloadedConfirmations(const QList<Qso> &confirmed, const QStringList &errors);
    void applyUploadedQsos           (const QList<Qso> &updated,   const QStringList &errors);

    void setupMenuBar();
    void setupCentralWidget();
    void setupStatusBar();

    void openDefaultDatabase();
    void reloadLog();
    void updateQsoCount();

    // Wire a radio backend's signals to the entry panel and status bar.
    // Call once per backend after construction.
    void wireRadioBackend(RadioBackend *backend);

    // Wire a digital listener's signals to the entry panel and auto-log logic.
    void wireDigitalListener(DigitalListenerService *svc);

    // Returns true if any radio backend is currently connected.
    bool anyRadioConnected() const;

    // Database + model
    std::unique_ptr<DatabaseInterface> m_db;
    QsoTableModel *m_logModel = nullptr;

    // Radio backends — typed members for menu-action slots; generic list for
    // shared wiring and disconnect-all.
    HamlibBackend          *m_hamlibBackend  = nullptr;
    TciBackend             *m_tciBackend     = nullptr;
    QList<RadioBackend*>    m_radioBackends;
    QList<QAction*>         m_radioConnectActions;  // all "Connect X" actions

    // QSL services — single registration list consumed by both dialogs
    LoTwService    *m_lotwService    = nullptr;
    EqslService    *m_eqslService    = nullptr;
    QrzService     *m_qrzService     = nullptr;
    ClubLogService *m_clublogService = nullptr;
    QList<QslService*> m_qslServices;

    // Digital listeners
    WsjtxService              *m_wsjtxService = nullptr;
    QList<DigitalListenerService*> m_digitalListeners;

    // Central layout
    RadioPanel    *m_radioPanel = nullptr;
    LogFilterBar  *m_filterBar  = nullptr;
    QSplitter     *m_splitter   = nullptr;
    QTableView    *m_logView    = nullptr;
    QsoEntryPanel *m_entryPanel = nullptr;

    // Status bar
    QLabel *m_hamlibIndicator  = nullptr;
    QLabel *m_tciIndicator     = nullptr;
    QLabel *m_wsjtxIndicator   = nullptr;
    QLabel *m_statusMsgLabel   = nullptr;
    QLabel *m_qsoCountLabel    = nullptr;

    enum class IndicatorState { Idle, Connected, Fault };
    static void setIndicatorState(QLabel *indicator, IndicatorState state);
    void showStatusMessage(const QString &msg, int ms = 0);

    // Actions
    QAction *m_newLogAction          = nullptr;
    QAction *m_importAdifAction      = nullptr;
    QAction *m_exportAdifAction      = nullptr;
    QAction *m_exitAction            = nullptr;
    QAction *m_settingsAction        = nullptr;
    QAction *m_aboutAction           = nullptr;
    QAction *m_whatsNewAction        = nullptr;
    QAction *m_connectHamlibAction   = nullptr;
    QAction *m_connectTciAction      = nullptr;
    QAction *m_disconnectRadioAction = nullptr;
};
