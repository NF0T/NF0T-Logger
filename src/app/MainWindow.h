#pragma once

#include <memory>
#include <QMainWindow>

class QAction;
class QLabel;
class QSplitter;
class QTableView;

class DatabaseInterface;
class HamlibBackend;
class Qso;
class QsoEntryPanel;
class QsoTableModel;
class TciBackend;

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
    void onConnectHamlib();
    void onConnectTci();
    void onDisconnectRadio();

private:
    void setupMenuBar();
    void setupToolBar();
    void setupCentralWidget();
    void setupStatusBar();

    void openDefaultDatabase();
    void reloadLog();
    void updateQsoCount();

    // Database + model
    std::unique_ptr<DatabaseInterface> m_db;
    QsoTableModel *m_logModel = nullptr;

    // Radio backends
    HamlibBackend *m_hamlibBackend = nullptr;
    TciBackend    *m_tciBackend    = nullptr;

    // Central layout
    QSplitter     *m_splitter   = nullptr;
    QTableView    *m_logView    = nullptr;
    QsoEntryPanel *m_entryPanel = nullptr;

    // Status bar labels
    QLabel *m_radioStatusLabel = nullptr;
    QLabel *m_qsoCountLabel    = nullptr;

    // Actions
    QAction *m_newLogAction          = nullptr;
    QAction *m_importAdifAction      = nullptr;
    QAction *m_exportAdifAction      = nullptr;
    QAction *m_exitAction            = nullptr;
    QAction *m_settingsAction        = nullptr;
    QAction *m_aboutAction           = nullptr;
    QAction *m_connectHamlibAction   = nullptr;
    QAction *m_connectTciAction      = nullptr;
    QAction *m_disconnectRadioAction = nullptr;
};
