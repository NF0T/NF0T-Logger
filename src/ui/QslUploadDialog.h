#pragma once

#include <QDialog>
#include <QList>
#include <QMetaObject>
#include <QStringList>

#include "core/logbook/Qso.h"

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QslService;
class DatabaseInterface;

/// Dialog for uploading QSOs to a selected QSL service.
///
/// Workflow:
///   1. User selects a service from the combo.
///   2. "Select Required" queries the DB for unsent QSOs for that service
///      and populates the table with all rows pre-selected.
///   3. User optionally deselects individual rows.
///   4. "Upload to <Service>" sends only the checked rows.
///   5. Live log shows per-QSO and server progress.
///
/// On completion, uploadCompleted() is emitted so MainWindow can update the DB.
class QslUploadDialog : public QDialog
{
    Q_OBJECT

public:
    QslUploadDialog(const QList<QslService*> &services,
                    DatabaseInterface        *db,
                    QWidget                  *parent = nullptr);

signals:
    void uploadCompleted(const QList<Qso> &updated, const QStringList &errors);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onServiceChanged(int index);
    void onSelectRequired();
    void onUploadClicked();
    void onUploadFinished(const QList<Qso> &updated, const QStringList &errors);

private:
    void appendLog(const QString &msg);
    void setRunning(bool running);
    void disconnectService();
    void updateUploadButton();

    QList<QslService*>             m_services;
    DatabaseInterface             *m_db             = nullptr;
    QslService                    *m_currentService = nullptr;
    QList<Qso>                     m_tableQsos;   // QSOs currently shown in table
    bool                           m_running       = false;
    QList<QMetaObject::Connection> m_serviceConns;

    QComboBox      *m_serviceCombo   = nullptr;
    QPushButton    *m_selectBtn      = nullptr;
    QTableWidget   *m_table          = nullptr;
    QPushButton    *m_uploadBtn      = nullptr;
    QPlainTextEdit *m_log            = nullptr;
    QLabel         *m_statusLabel    = nullptr;
    QPushButton    *m_closeBtn       = nullptr;
};
