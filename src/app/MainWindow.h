#pragma once

#include <QMainWindow>

class QAction;
class QLabel;
class QSplitter;
class QTableView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onNewLog();
    void onImportAdif();
    void onExportAdif();
    void onSettingsDialog();
    void onAbout();

private:
    void setupMenuBar();
    void setupToolBar();
    void setupCentralWidget();
    void setupStatusBar();

    // Central layout
    QSplitter   *m_splitter   = nullptr;
    QTableView  *m_logView    = nullptr;

    // Status bar labels
    QLabel *m_radioStatusLabel = nullptr;
    QLabel *m_qsoCountLabel    = nullptr;

    // Actions
    QAction *m_newLogAction    = nullptr;
    QAction *m_importAdifAction = nullptr;
    QAction *m_exportAdifAction = nullptr;
    QAction *m_exitAction      = nullptr;
    QAction *m_settingsAction  = nullptr;
    QAction *m_aboutAction     = nullptr;
};
