// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <memory>
#include <QFutureWatcher>
#include <QList>
#include <QMainWindow>
#include <QStringList>
#include <QVariantMap>

#include "core/logbook/Qso.h"
#include "lookup/CallsignLookupResult.h"

class QAction;
class QLabel;
class QTableView;
class QTimer;

/// Outcome of a background ADIF import (see MainWindow::onImportAdif()).
struct AdifImportResult {
    int imported   = 0;
    int duplicates = 0;
    int errors     = 0;
    int skipped    = 0;
    QStringList errorDetails;
};

class MigrateDatabaseDialog;
class QsoFullEntryDialog;
class CtyDatLookupProvider;
class QrzXmlLookupProvider;
class ClubLogService;
class DatabaseInterface;
class DigitalListenerService;
class EqslService;
class HamlibBackend;
class LoTwService;
class QrzService;
class QslService;
class LogFilterBar;
class QsoQuickEntryPanel;
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
    // Tools
    void onMigrateDatabase();

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

    // Builds the config map for the currently-configured backend (Settings),
    // and reports which backend key ("sqlite" | "mariadb") it belongs to.
    // Only call this to (re)open m_db — openDefaultDatabase() caches the
    // result into m_activeDbConfig/m_activeDbBackendKey. Anything else that
    // needs "the database m_db is connected to" (e.g. the background ADIF
    // import, which opens its own second connection to the same database)
    // must read those cached members instead of calling this again, since
    // Settings can change before the app restarts — see their declaration.
    QVariantMap currentBackendConfig(QString &keyOut) const;

    // Wire a radio backend's signals to the entry panel and status bar.
    // Call once per backend after construction.
    void wireRadioBackend(RadioBackend *backend);

    // Wire a digital listener's signals to the entry panel and auto-log logic.
    void wireDigitalListener(DigitalListenerService *svc);

    // Apply non-empty lookup result fields to any empty fields in qso.
    // Used in the WSJT-X auto-log path where the panel may already be cleared.
    static void applyLookupResult(Qso &qso, const CallsignLookupResult &r);

    // Disable/enable UI entry points while a database migration is running.
    void setMigrationLock(bool locked);

    // Disable/enable UI entry points that read or write m_db while a
    // background ADIF import is running. Deliberately separate from
    // setMigrationLock(): an import keeps its own DB connection (m_db is
    // untouched), so — unlike migration — there is no need to pause digital
    // listeners. Each lock disables the other's trigger action so the two
    // operations can't run concurrently.
    void setImportLock(bool locked);

    // Merge a QRZ result on top of a CTY.dat result following precedence rules:
    // CTY.dat is authoritative for zone/DXCC/entity; QRZ fills personal data
    // and overrides lat/lon/gridsquare with the operator's precise location.
    static CallsignLookupResult mergeQrzIntoCty(const CallsignLookupResult &base,
                                                const CallsignLookupResult &qrz);

    // Wire callsign lookup: CTY.dat (immediate) + QRZ debounce → entry panel.
    void wireCallsignLookup();

    // Returns true if any radio backend is currently connected.
    bool anyRadioConnected() const;

    // Database + model
    std::unique_ptr<DatabaseInterface> m_db;
    QsoTableModel *m_logModel = nullptr;

    // The backend key/config m_db was actually opened with, cached at open
    // time (openDefaultDatabase()). Settings::dbBackend()/resolvedSqlitePath()
    // can change the moment a user edits Settings and clicks OK — Database
    // settings only take effect after a restart, so anything targeting "the
    // currently open database" (e.g. the ADIF import worker) must read these
    // cached values rather than re-deriving from Settings, or it would target
    // a different file/server than m_db is connected to.
    QString m_activeDbBackendKey;
    QVariantMap m_activeDbConfig;

    // Background ADIF import (see onImportAdif()). Non-null while an import
    // is in flight; closeEvent() refuses to close the window until it clears.
    QFutureWatcher<AdifImportResult> *m_adifImportWatcher = nullptr;

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

    // Callsign lookup
    CtyDatLookupProvider *m_ctyProvider          = nullptr;
    QrzXmlLookupProvider *m_qrzProvider          = nullptr;
    QTimer               *m_callsignLookupTimer  = nullptr;
    QString               m_pendingLookupCallsign;
    CallsignLookupResult  m_cachedLookupResult;   // survives clearForm() for WSJT-X enrichment

    // Central layout
    RadioPanel           *m_radioPanel = nullptr;
    QsoQuickEntryPanel   *m_entryPanel = nullptr;
    LogFilterBar         *m_filterBar  = nullptr;
    QTableView           *m_logView    = nullptr;

    // Status bar
    QLabel *m_hamlibIndicator  = nullptr;
    QLabel *m_tciIndicator     = nullptr;
    QLabel *m_wsjtxIndicator   = nullptr;
    QLabel *m_statusMsgLabel   = nullptr;
    QLabel *m_qsoCountLabel    = nullptr;

    enum class IndicatorState { Idle, Connected, Fault };
    static void setIndicatorState(QLabel *indicator, IndicatorState state);
    void showStatusMessage(const QString &msg, int ms = 0);

    bool m_migrationLock = false;
    bool m_importLock    = false;

    // Actions
    QAction *m_newQsoAction            = nullptr;
    QAction *m_newLogAction            = nullptr;
    QAction *m_importAdifAction        = nullptr;
    QAction *m_exportAdifAction        = nullptr;
    QAction *m_exitAction              = nullptr;
    QAction *m_settingsAction          = nullptr;
    QAction *m_aboutAction             = nullptr;
    QAction *m_whatsNewAction          = nullptr;
    QAction *m_connectHamlibAction     = nullptr;
    QAction *m_connectTciAction        = nullptr;
    QAction *m_disconnectRadioAction   = nullptr;
    QAction *m_qslDownloadAction       = nullptr;
    QAction *m_qslUploadAction         = nullptr;
    QAction *m_migrateDatabaseAction   = nullptr;
};
