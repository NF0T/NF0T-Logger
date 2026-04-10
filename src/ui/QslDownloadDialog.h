#pragma once

#include <QDate>
#include <QDialog>
#include <QList>
#include <QMetaObject>
#include <QStringList>

#include "core/logbook/Qso.h"

class QComboBox;
class QDateEdit;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QslService;

/// Non-modal dialog for downloading QSL confirmations from a selected service.
///
/// The user picks a service, adjusts the date range (defaults: last-downloaded
/// date for that service → today), then clicks Download. Progress and server
/// responses appear in the log area.
///
/// On completion, downloadCompleted() is emitted so the caller can update
/// the database with the matched confirmations.
class QslDownloadDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QslDownloadDialog(const QList<QslService*> &services,
                               const QList<Qso>         &localQsos,
                               QWidget                  *parent = nullptr);

signals:
    /// Emitted when matching is complete. matched contains the local Qso
    /// records with rcvd fields updated — ready for direct DB write.
    void downloadCompleted(const QList<Qso> &matched, const QStringList &errors);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onServiceChanged(int index);
    void onDownloadClicked();
    void onDownloadFinished(const QList<Qso> &confirmed, const QStringList &errors);

private:
    void appendLog(const QString &msg);
    void setRunning(bool running);
    void disconnectService();

    QList<QslService*>             m_services;
    QList<Qso>                     m_localQsos;
    QslService                    *m_currentService = nullptr;
    bool                           m_running        = false;
    QList<QMetaObject::Connection> m_serviceConns;

    QComboBox      *m_serviceCombo = nullptr;
    QDateEdit      *m_fromDate     = nullptr;
    QDateEdit      *m_toDate       = nullptr;
    QPlainTextEdit *m_log          = nullptr;
    QLabel         *m_statusLabel  = nullptr;
    QPushButton    *m_downloadBtn  = nullptr;
    QPushButton    *m_closeBtn     = nullptr;
};
